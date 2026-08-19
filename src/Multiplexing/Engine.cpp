#include "../../includes/multiplexing/header.hpp"
#include "../../includes/Request/ClientRequest.hpp"
#include "../../includes/Response/Dispatcher.hpp"
#include "../Logging/Logging.hpp"

volatile sig_atomic_t loop_is_true = 1;

// ---------------------------------------- AFd Class ---------------------------------------- //

AFd::AFd() : fd(-1) {}

AFd::~AFd()
{
    if (fd != -1)
        close(fd);
}

int AFd::get_fd() const
{
    return fd;
}


// ---------------------------------------- Socket Class ------------------------------------- //

Socket::Socket(Server_block& obj) : _port(0), server(obj)
{
}


bool Multiplexer::is_cgi(const std::string& path, const Location_Config& location)
{
    size_t pos = path.rfind('.');
    if (pos == std::string::npos)
        return false;
    std::string ext = path.substr(pos);
    for (size_t i = 0; i < location.cgi_extensions.size(); ++i)
    {
        if (location.cgi_extensions[i] == ext)
            return true;
    }
    return false;
}


void Multiplexer::registerCgiPipes(Client& client)
{
    _cgi_pipes[client.cgi.stdout_fd] = client.fd;
    addFd(client.cgi.stdout_fd, EPOLLIN);
    _cgi_pipes[client.cgi.stdin_fd] = client.fd;
    addFd(client.cgi.stdin_fd, EPOLLOUT);
}

bool Multiplexer::startCgi(Client& client, const Server_block& server)
{
    const Location_Config* location =
        Router::resolveLocation(client.parsed_request.getRequestPath(), server);

    if (location == NULL)
        return false;

    CGI cgi(client, *location, server);

    if (!cgi.isRunnable() || !cgi.execute())
        return false;

    client.cgi.pid = cgi.get_pid();
    client.cgi.stdin_fd = cgi.get_input_fd();
    client.cgi.stdout_fd = cgi.get_output_fd();
    client.cgi.running = true;
    client.cgi.last_activity = time(NULL);
    openCgiBodySource(client);
    registerCgiPipes(client);
    return true;
}


void Multiplexer::releaseCgi(Client& client)
{
    closeCgiInput(client);
    if (client.cgi.stdout_fd != -1)
    {
        removeFd(client.cgi.stdout_fd);
        _cgi_pipes.erase(client.cgi.stdout_fd);
        close(client.cgi.stdout_fd);
        client.cgi.stdout_fd = -1;
    }
    if (client.cgi.pid != -1)
    {
        kill(client.cgi.pid, SIGKILL);
        waitpid(client.cgi.pid, NULL, 0);
        client.cgi.pid = -1;
    }
    client.cgi.running = false;
}


void Multiplexer::prepareResponse(Client &client)
{
    if (client.response_prepared)
        return;

    const Server_block& server = client.Client_server;
    Response response = Dispatcher::dispatch(client, server);

    client.response_prepared = true;
    if (!response.isCGI())
    {
        response.addHeader("Connection", "close");
        client.response = response.toString();
        return;
    }
    if (startCgi(client, server))
        return;

    client.response = AMethod::buildErrorResponse(HTTP_502_BAD_GATEWAY, "Bad Gateway").toString();
}


bool Multiplexer::sendResponse(int fd, Client &client)
{
    if (client.response.empty())
        return true;

    ssize_t sent = send(fd, client.response.data(), client.response.size(), MSG_NOSIGNAL);
    if (sent <= 0)
    {
        _removeClient(fd);
        return false;
    }
    client.response.erase(0, sent);
    return client.response.empty();
}

void    Multiplexer::sendStreaming(int fd, Client &client)
{
    if (client.stream_buffer_offset == client.stream_buffer_size)
    {
        client.stream_buffer_size = read(client.stream_file_fd,client.stream_buffer,sizeof(client.stream_buffer));
        client.stream_buffer_offset = 0;
        if (client.stream_buffer_size <= 0)
        {
            close(client.stream_file_fd);
            client.stream_file_fd = -1;
            client.stream_bytes_remaining = 0;
            return;
        }
    }
    ssize_t sent = send(fd,client.stream_buffer + client.stream_buffer_offset,client.stream_buffer_size - client.stream_buffer_offset,MSG_NOSIGNAL);
    if (sent <= 0)
    {
        close(client.stream_file_fd);
        _removeClient(fd);
        return;
    }
    client.stream_buffer_offset += sent;
    client.stream_bytes_remaining -= sent;
}


void Multiplexer::killTimedOutCgi()
{
    std::map<int, Client>::iterator it;
    time_t now = time(NULL);

    for (it = _clients.begin(); it != _clients.end(); ++it)
    {
        Client& client = it->second;

        if (!client.cgi.running || now - client.cgi.last_activity <= CGI_TIMEOUT)
            continue;
        if (client.cgi.headers_done)
            client.response += "0\r\n\r\n";
        else
        {
            Response response = AMethod::buildErrorResponse(HTTP_504_GATEWAY_TIMEOUT, "Gateway Timeout");
            Dispatcher::setErrorPageBody(response, client.Client_server);
            client.response = response.toString();
        }
        releaseCgi(client);
        enableWrite(client.fd);
    }
}

static bool isStatusHeader(const std::string& line)
{
    return line.size() >= 7 && strncasecmp(line.c_str(), "Status:", 7) == 0;
}

static std::vector<std::string> splitHeaderLines(const std::string& block)
{
    std::vector<std::string> lines;
    size_t start = 0;

    while (start < block.size())
    {
        size_t end = block.find('\n', start);
        std::string line = (end == std::string::npos)
                         ? block.substr(start)
                         : block.substr(start, end - start);

        if (!line.empty() && line[line.size() - 1] == '\r')
            line.erase(line.size() - 1);
        if (!line.empty())
            lines.push_back(line);
        if (end == std::string::npos)
            break;
        start = end + 1;
    }
    return lines;
}

static std::string cgiStatusLine(const std::vector<std::string>& lines)
{
    for (size_t i = 0; i < lines.size(); i++)
    {
        if (!isStatusHeader(lines[i]))
            continue;

        std::string value = lines[i].substr(7);
        size_t begin = value.find_first_not_of(" \t");

        if (begin != std::string::npos)
            return value.substr(begin);
    }
    return "200 OK";
}

static std::string cgiForwardedHeaders(const std::vector<std::string>& lines)
{
    std::string headers;

    for (size_t i = 0; i < lines.size(); i++)
    {
        if (isStatusHeader(lines[i]))
            continue;
        headers += lines[i] + "\r\n";
    }
    return headers;
}

static std::string buildCgiResponseHead(const std::string& cgiHeaderBlock)
{
    std::vector<std::string> lines = splitHeaderLines(cgiHeaderBlock);
    std::string head;

    head += "HTTP/1.1 " + cgiStatusLine(lines) + "\r\n";
    head += cgiForwardedHeaders(lines);
    head += "Transfer-Encoding: chunked\r\n";
    head += "Connection: close\r\n";
    head += "\r\n";
    return head;
}

static void appendChunk(std::string& out, const char* data, size_t size)
{
    std::ostringstream header;

    if (size == 0)
        return;
    header << std::hex << size << "\r\n";
    out += header.str();
    out.append(data, size);
    out += "\r\n";
}

static size_t findHeaderBlockEnd(const std::string& buffer, size_t& terminatorSize)
{
    size_t position = buffer.find("\r\n\r\n");

    if (position != std::string::npos)
    {
        terminatorSize = 4;
        return position;
    }
    position = buffer.find("\n\n");
    if (position != std::string::npos)
    {
        terminatorSize = 2;
        return position;
    }
    return std::string::npos;
}

void Multiplexer::emitCgiHeaders(Client& client)
{
    size_t terminatorSize = 0;
    size_t end = findHeaderBlockEnd(client.cgi.header_buffer, terminatorSize);

    if (end == std::string::npos)
    {
        if (client.cgi.header_buffer.size() < MAX_HEADER_SIZE)
            return;
        end = 0;
        terminatorSize = 0;
    }

    std::string headerBlock = client.cgi.header_buffer.substr(0, end);
    std::string payload = client.cgi.header_buffer.substr(end + terminatorSize);

    client.response += buildCgiResponseHead(headerBlock);
    client.cgi.headers_done = true;
    client.cgi.header_buffer.clear();
    appendChunk(client.response, payload.data(), payload.size());
}


void Multiplexer::_writeClient(int fd)
{
    Client* client = findClient(fd);

    if (client == NULL)
        return;

    prepareResponse(*client);
    if (!sendResponse(fd, *client))
        return;
    if (client->stream_file_fd != -1)
    {
        sendStreaming(fd, *client);
        return;
    }
    if (client->cgi.running)
    {
        disableWrite(fd);
        return;
    }
    _removeClient(fd);
}


void Socket::setup(int port, const std::string& host)
{
    _port = port;
    _host = host;
    this->fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd == -1)
        throw Error::Socket();
    int opt = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    fcntl(fd, F_SETFD, FD_CLOEXEC);
    fcntl(fd, F_SETFL, O_NONBLOCK);
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = inet_addr(host.c_str());

    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) == -1)
        throw Error::Bind();
    if (listen(fd, SOMAXCONN) == -1)
        throw Error::Listen();
}

int Socket::get_listen_port()
{
    return _port;
}


Socket::~Socket()
{
}



Multiplexer::Multiplexer() : _epoll_fd(-1)
{
    _epoll_fd = epoll_create(MAX_EVENTS);
    if (_epoll_fd == -1)
        throw Error::Epoll();
    fcntl(_epoll_fd, F_SETFD, FD_CLOEXEC);
}

Multiplexer::~Multiplexer()
{
    for (size_t i = 0; i < _servers.size(); i++)
        delete _servers[i];
    if (_epoll_fd != -1)
        close(_epoll_fd);
}


void Multiplexer::addServer(Socket *s)
{
    _servers.push_back(s);
    addFd(s->get_fd(), EPOLLIN);
}


void Multiplexer::addFd(int fd, uint32_t events)
{
    struct epoll_event entry;

    memset(&entry, 0, sizeof(entry));
    entry.events = events;
    entry.data.fd = fd;
    if (epoll_ctl(_epoll_fd, EPOLL_CTL_ADD, fd, &entry) == -1)
        return;
    _watched[fd] = events;
}

void Multiplexer::removeFd(int fd)
{
    if (_watched.find(fd) == _watched.end())
        return;
    epoll_ctl(_epoll_fd, EPOLL_CTL_DEL, fd, NULL);
    _watched.erase(fd);
    _dead_fds.insert(fd);
}


bool Multiplexer::isRegistered(int fd) const
{
    return _watched.find(fd) != _watched.end();
}


void Multiplexer::disableWrite(int fd)
{
    std::map<int, uint32_t>::iterator it = _watched.find(fd);

    if (it == _watched.end())
        return;
    setEvents(fd, it->second & ~static_cast<uint32_t>(EPOLLOUT));
}

void Multiplexer::_acceptNewClient(Socket *s)
{
    struct sockaddr_in client_addr;
    socklen_t len = sizeof(client_addr);
    int client_fd = accept(s->get_fd(), (struct sockaddr *)&client_addr, &len);
    if (client_fd == -1)
        return;

    fcntl(client_fd, F_SETFL, O_NONBLOCK);
    fcntl(client_fd, F_SETFD, FD_CLOEXEC);

    if (_clients.size() >= MAX_CLIENTS)
        evictOldestClient();

    Client client(s->server);
    client.fd = client_fd;
    client.port = s->get_listen_port();
    client.last_activity = time(NULL);
    client.parsed_request.state = ClientRequest::HEADERS;
    _clients.insert(std::make_pair(client_fd, client));
    addFd(client_fd, EPOLLIN);
}


std::string get_listen_value(const std::string& host)
{
    size_t pos = host.find(":");
    if (pos != std::string::npos)
        return host.substr(pos + 1);
    else
        return "";
}


void read_and_print_fd(int fd)
{
    char buffer[1024];
    ssize_t bytes_read;

    if (fd < 0)
        return;
    while ((bytes_read = read(fd, buffer, sizeof(buffer))) > 0)
    {
        if (bytes_read == 0)
            std::cout << "error in fd empty\n";
        ssize_t bytes_written = 0;
        while (bytes_written < bytes_read)
        {
            ssize_t ret = write(STDOUT_FILENO, buffer + bytes_written, bytes_read - bytes_written);
            if (ret <= 0)
            {
                std::cerr << "Error writing to stdout" << std::endl;
                return;
            }
            bytes_written += ret;
        }
    }

    if (bytes_read < 0)
        std::cerr << "Error reading from file descriptor" << std::endl;
}

bool Response::isCGI() const
{
    return _mode == CGI_RESPONSE;
}


void Multiplexer::enableWrite(int fd)
{
    setEvents(fd, EPOLLOUT);
}

void Multiplexer::_removeClient(int fd)
{
    std::map<int, Client>::iterator it = _clients.find(fd);

    if (it == _clients.end())
        return;
    releaseCgi(it->second);
    if (it->second.stream_file_fd != -1) {
        close(it->second.stream_file_fd);
    }
    removeFd(fd);
    close(fd);
    _clients.erase(it);
}


void Multiplexer::handlePeerShutdown(int fd, Client& client)
{
    ClientRequest& request = client.parsed_request;

    if (request.state == ClientRequest::HEADERS && client.request.empty())
    {
        _removeClient(fd);
        return;
    }
    if (request.state != ClientRequest::DONE && request.state != ClientRequest::ERROR_STATE)
    {
        request.setStatusCode(400);
        request.state = ClientRequest::ERROR_STATE;
    }
    enableWrite(fd);
}


void Multiplexer::_readClient(int fd)
{
    char buffer[4096];
    std::map<int, Client>::iterator iter = _clients.find(fd);
	if (iter != _clients.end())
    {
        int bytesRead = recv(fd, buffer, sizeof(buffer), 0);
        if (bytesRead == -1)
        {
            _removeClient(fd);
            return;
        }
        else if (bytesRead > 0)
        {
            iter->second.request.append(buffer, bytesRead);
            iter->second.parsed_request.parse(iter->second);
            if (iter->second.parsed_request.state == ClientRequest::BODY)
                iter->second.parsed_request.BodyRequest(iter->second);

        }
        else if (bytesRead == 0)
        {
            handlePeerShutdown(fd, iter->second);
            return;
        }
        if (iter->second.parsed_request.state == ClientRequest::DONE || iter->second.parsed_request.state == ClientRequest::ERROR_STATE)
            enableWrite(fd);
    }
}

void Multiplexer::dispatchEvents(struct epoll_event* events, int count)
{
    _dead_fds.clear();
    for (int i = 0; i < count; i++)
        handleEvent(events[i].data.fd, events[i].events);
}

void Multiplexer::handleEvent(int fd, uint32_t revents)
{
    if (_dead_fds.find(fd) != _dead_fds.end())
        return;
    if (!isRegistered(fd))
        return;
    if (handleServerEvent(fd, revents))
        return;
    if (handleCgiEvent(fd, revents))
        return;
    handleClientEvent(fd, revents);
}

bool Multiplexer::handleServerEvent(int fd, uint32_t revents)
{
    for (size_t i = 0; i < _servers.size(); i++)
    {
        if (_servers[i]->get_fd() != fd)
            continue;
        if (revents & EPOLLIN)
            _acceptNewClient(_servers[i]);
        return true;
    }
    return false;
}

bool Multiplexer::handleCgiEvent(int fd, uint32_t revents)
{
    if (_cgi_pipes.find(fd) == _cgi_pipes.end())
        return false;

    Client* client = findClientByPipe(fd);
    if (client == NULL)
    {
        removeFd(fd);
        _cgi_pipes.erase(fd);
        close(fd);
        return true;
    }
    if (fd == client->cgi.stdin_fd)
    {
        if (revents & (EPOLLERR | EPOLLHUP))
            closeCgiInput(*client);
        else if (revents & EPOLLOUT)
            writeCgiInput(fd);
        return true;
    }
    if (revents & (EPOLLIN | EPOLLHUP | EPOLLERR))
        readCgiOutput(fd);
    return true;
}


void Multiplexer::handleClientEvent(int fd, uint32_t revents)
{
    if (revents & EPOLLIN)
        _readClient(fd);
    if (findClient(fd) != NULL && (revents & EPOLLOUT))
        _writeClient(fd);
    if (findClient(fd) != NULL && (revents & (EPOLLHUP | EPOLLERR)))
        _removeClient(fd);
}

void Multiplexer::run()
{
    struct epoll_event events[MAX_EVENTS];

    while (loop_is_true)
    {
        killTimedOutCgi();
        closeIdleClients();
        applyCgiBackPressure();

        int ready = epoll_wait(_epoll_fd, events, MAX_EVENTS, EPOLL_TIMEOUT_MS);
        if (ready < 0)
        {
            if (errno == EINTR)
                break;
            throw Error::Epoll();
        }
        if (ready == 0)
            continue;
        dispatchEvents(events, ready);
    }
}


Client* Multiplexer::findClient(int fd)
{
    std::map<int, Client>::iterator it = _clients.find(fd);

    if (it == _clients.end())
        return NULL;
    return &it->second;
}


Client* Multiplexer::findClientByPipe(int pipe_fd)
{
    std::map<int, int>::iterator it = _cgi_pipes.find(pipe_fd);

    if (it == _cgi_pipes.end())
        return NULL;
    return findClient(it->second);
}


void Multiplexer::advanceRequest(int fd, Client& client)
{
    client.parsed_request.parse(client);
    if (client.parsed_request.state == ClientRequest::BODY)
        client.parsed_request.BodyRequest(client);
    if (client.parsed_request.state == ClientRequest::DONE
        || client.parsed_request.state == ClientRequest::ERROR_STATE)
        enableWrite(fd);
}

void Multiplexer::setEvents(int fd, uint32_t events)
{
    std::map<int, uint32_t>::iterator it = _watched.find(fd);

    if (it == _watched.end() || it->second == events)
        return;                              /* nothing to say to the kernel */

    struct epoll_event entry;

    memset(&entry, 0, sizeof(entry));
    entry.events = events;
    entry.data.fd = fd;
    if (epoll_ctl(_epoll_fd, EPOLL_CTL_MOD, fd, &entry) == -1)
        return;
    it->second = events;
}

bool Multiplexer::isEvictable(const Client& client) const
{
    return !client.cgi.running
        && client.response.empty()
        && client.stream_file_fd == -1;
}


void Multiplexer::evictOldestClient()
{
    std::map<int, Client>::iterator it;
    int oldest = -1;
    time_t oldestSeen = 0;

    for (it = _clients.begin(); it != _clients.end(); ++it)
    {
        if (!isEvictable(it->second))
            continue;
        if (oldest == -1 || it->second.last_activity < oldestSeen)
        {
            oldest = it->first;
            oldestSeen = it->second.last_activity;
        }
    }
    if (oldest == -1)
        return;
    _removeClient(oldest);
}


void Multiplexer::closeIdleClients()
{
    std::map<int, Client>::iterator it = _clients.begin();
    std::vector<int> expired;
    time_t now = time(NULL);

    while (it != _clients.end())
    {
        const Client& client = it->second;

        if (client.parsed_request.state == ClientRequest::HEADERS
            && client.request.empty()
            && isEvictable(client)
            && now - client.last_activity > CLIENT_IDLE_TIMEOUT)
            expired.push_back(it->first);
        ++it;
    }
    for (size_t i = 0; i < expired.size(); i++)
        _removeClient(expired[i]);
}
