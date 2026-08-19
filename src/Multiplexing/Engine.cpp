#include "../../includes/multiplexing/header.hpp"
#include "../../includes/Request/ClientRequest.hpp"
// #include "../../includes/Request/RequestHelpers.hpp"
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
    INFO() << "Multiplexer::startCgi: started pid=" << client.cgi.pid
           << " script=" << cgi.get_script() << " client fd=" << client.fd;
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
        DEBUG("Multiplexer") << "releaseCgi: reaped cgi pid=" << client.cgi.pid;
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

    WARN() << "Multiplexer::prepareResponse: cgi start failed fd=" << client.fd;
    client.response = AMethod::buildErrorResponse(HTTP_502_BAD_GATEWAY, "Bad Gateway").toString();
}


bool Multiplexer::sendResponse(int fd, Client &client)
{
    if (client.response.empty())
        return true;

    ssize_t sent = send(fd, client.response.data(), client.response.size(), MSG_NOSIGNAL);
    if (sent <= 0)
    {
        WARN() << "Multiplexer::sendResponse: send failed to client fd=" << fd
               << ": " << strerror(errno);
        _removeClient(fd);
        return false;
    }
    client.response.erase(0, sent);
    DDEBUG("Multiplexer") << "sendResponse: wrote " << sent << " bytes to fd=" << fd
                          << " remaining=" << client.response.size();
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
			DEBUG("Multiplexer") << "sendStreaming: finished streaming, closed stream file fd="
			                     << client.stream_file_fd << " client fd=" << fd;
            close(client.stream_file_fd);
            client.stream_file_fd = -1;
            client.stream_bytes_remaining = 0;
            return;
        }
    }
    ssize_t sent = send(fd,client.stream_buffer + client.stream_buffer_offset,client.stream_buffer_size - client.stream_buffer_offset,MSG_NOSIGNAL);
    if (sent <= 0)
    {
		WARN() << "Multiplexer::sendStreaming: send failed to client fd=" << fd
		       << ": " << strerror(errno) << ", closing connection";
        close(client.stream_file_fd);
        _removeClient(fd);
        return;
    }
    client.stream_buffer_offset += sent;
    client.stream_bytes_remaining -= sent;
    DDEBUG("Multiplexer") << "sendStreaming: wrote " << sent << " bytes to fd=" << fd
                          << " remaining=" << client.stream_bytes_remaining;
}

// void    Multiplexer::disableWrite(int fd)
// {
//     for (size_t i = 0; i < _pollfds.size(); i++)
//     {
//         if (_pollfds[i].fd == fd)
//         {
//             _pollfds[i].events &= ~POLLOUT;
//             break;
//         }
//     }
// }

/////////////////////////// 13 : cgi timeout //////////////////////////////

void Multiplexer::killTimedOutCgi()
{
    std::map<int, Client>::iterator it;
    time_t now = time(NULL);

    for (it = _clients.begin(); it != _clients.end(); ++it)
    {
        Client& client = it->second;

        if (!client.cgi.running || now - client.cgi.last_activity <= CGI_TIMEOUT)
            continue;
        WARN() << "Multiplexer::killTimedOutCgi: cgi pid=" << client.cgi.pid
               << " idle for " << (now - client.cgi.last_activity)
               << "s, responding status=504 client fd=" << client.fd;
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
///////////////////////////////////////////////////////////////////////////

/////////////////// 14 : cgi output is not http ////////////////////////////////

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

///////////////////////////////////////////////////////////////////////////

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
    {
        ERR() << "Socket::setup: socket failed host=" << host << " port=" << port
              << ": " << strerror(errno);
        throw Error::Socket();
    }
    DEBUG("Socket") << "setup: opened listening socket fd=" << fd;
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
    {
        ERR() << "Socket::setup: bind failed on " << host << ":" << port
              << " fd=" << fd << ": " << strerror(errno);
        throw Error::Bind();
    }
    if (listen(fd, SOMAXCONN) == -1)
    {
        ERR() << "Socket::setup: listen failed on " << host << ":" << port
              << " fd=" << fd << ": " << strerror(errno);
        throw Error::Listen();
    }
    DEBUG("Socket") << "setup: listening on " << host << ":" << port << " fd=" << fd;
}

int Socket::get_listen_port()
{
    return _port;
}


Socket::~Socket()
{
	// DEBUG("Socket") << "~Socket: closed listening socket fd=" << fd;
    // close(fd);
}


// ------------------------------------------- Multiplexer Class ------------------------------ //


///////////////////////////////////////////////// 31: epoll-eventloop ////////////////////////////////////////////////////


// Multiplexer::Multiplexer() {}

Multiplexer::Multiplexer() : _epoll_fd(-1)
{
    _epoll_fd = epoll_create(MAX_EVENTS);
    if (_epoll_fd == -1)
    {
        ERR() << "Multiplexer::Multiplexer: epoll_create failed: " << strerror(errno);
        throw Error::Epoll();
    }
    fcntl(_epoll_fd, F_SETFD, FD_CLOEXEC);
    DEBUG("Multiplexer") << "Multiplexer: epoll instance fd=" << _epoll_fd;
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
    {
        WARN() << "Multiplexer::addFd: epoll_ctl ADD failed fd=" << fd
               << ": " << strerror(errno);
        return;
    }
    _watched[fd] = events;
    DDEBUG("Multiplexer") << "addFd: watching fd=" << fd << " events=" << events
                          << " total=" << _watched.size();
}

void Multiplexer::removeFd(int fd)
{
    if (_watched.find(fd) == _watched.end())
        return;
    if (epoll_ctl(_epoll_fd, EPOLL_CTL_DEL, fd, NULL) == -1)
    {
        WARN() << "Multiplexer::removeFd: epoll_ctl DEL failed fd=" << fd
               << ": " << strerror(errno);
    }
    _watched.erase(fd);
    _dead_fds.insert(fd);
    DDEBUG("Multiplexer") << "removeFd: stopped watching fd=" << fd
                          << " total=" << _watched.size();
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





// void Multiplexer::_acceptNewClient(Socket *s)
// {
//     struct sockaddr_in client_id;
//     socklen_t len = sizeof(client_id);
//     int client_fd = accept(s->get_fd(), (struct sockaddr *)&client_id, &len);
//     if (client_fd == -1)
//     {
//         ERR() << "Multiplexer::_acceptNewClient: accept failed on listening fd=" << s->get_fd()
//               << ": " << strerror(errno);
//         return;
//     }
// 	DEBUG("Multiplexer") << "_acceptNewClient: accepted client fd=" << client_fd
// 	                     << " port=" << s->get_listen_port();
//     fcntl(client_fd, F_SETFL, O_NONBLOCK);

//     Client client;

//     client.fd = client_fd;
//     client.port = s->get_listen_port();
//     client.parsed_request.state = ClientRequest::HEADERS;
//     _clients[client_fd] = client;

//     struct pollfd pfd;
//     pfd.fd = client_fd;
//     pfd.events = POLLIN;
//     pfd.revents = 0;
//     _pollfds.push_back(pfd);
// }


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
    // DEBUG("Multiplexer") << "_acceptNewClient: accepted fd=" << client_fd
    //                      << " port=" << s->get_listen_port()
    //                      << " total=" << _clients.size();
}


std::string get_listen_value(const std::string& host)
{
    size_t pos = host.find(":");
    if (pos != std::string::npos)
        return host.substr(pos + 1);
    else
        return "";
}

// static std::string get_header_value(const std::map<std::string, std::string>& headers, const std::string& key)
// {
//     std::map<std::string, std::string>::const_iterator it = headers.find(key);
//     if (it == headers.end())
//         return "";
//     return it->second;
// }

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

// int Multiplexer::handleClient(int fd)
// {
//     size_t server_index = 0;
//     for (size_t i = 0; i < Conf_File::Servers.size(); i++)
//     {
//         if (get_listen_value(get_header_value(_clients[fd].parsed_request.getHeaders(), "host")) == Conf_File::Servers[i].listen_port_str[0])
//         {
//             server_index = i;
//             break;
//         }
//     }
//     size_t location_index = 0;
//     size_t longest_match = 0;
//     for (size_t i = 0; i < Conf_File::Servers[server_index].location.size(); i++)
//     {
//         std::string loc_path = Conf_File::Servers[server_index].location[i].path;
//         if (_clients[fd].parsed_request.getRequestPath().find(loc_path) == 0 && loc_path.size() > longest_match)
//         {
//             longest_match = loc_path.size();
//             location_index = i;
//         }
//     }
//     Location_Config& loc = Conf_File::Servers[server_index].location[location_index];
//     std::string req_path = _clients[fd].parsed_request.getRequestPath();
//     size_t dot_pos = req_path.rfind('.');
//     if (dot_pos == std::string::npos)
//         return 0;

//     std::string extension = req_path.substr(dot_pos);
//     for (size_t i = 0; i < loc.cgi_extensions.size(); i++)
//     {
//         if (loc.cgi_extensions[i] == extension)
//         {
//             CGI cgi(_clients[fd], loc);
//             int pipe_fd = cgi.execute(_cgi_pids);
//             cgi_timeouts[pipe_fd] = time(NULL);
//             if (pipe_fd == -1)
//                 return ERROR;
//             cgi.writeToChild();
//             // std::cout << "heres whats inside the pipe filled by cgi\n";
//             // read_and_print_fd(pipe_fd);
//             _cgi_pipes[pipe_fd] = fd;
//             struct pollfd pfd;
//             pfd.fd = pipe_fd;
//             pfd.events = POLLIN;
//             pfd.revents = 0;
//             _pollfds.push_back(pfd);
//             return 1;
//         }
//     }
//     return 0;
// }

// void Multiplexer::run()
// {
//     while (loop_is_true)  
//     {
// 		INFO() << "Multiplexer::run: running, waiting for events";
//         std::map<int , time_t>::iterator iter = cgi_timeouts.begin();
//         time_t current = time(NULL);
//         while (iter != cgi_timeouts.end())
//         {
// 			DEBUG("Multiplexer") << "Checking CGI timeout for pipe fd: " << iter->first << ", elapsed time: " << (current - iter->second) << " seconds";
//             if ((current - iter->second) > 5)
//             {
// 				DEBUG("Multiplexer") << "CGI timeout occurred for pipe fd: " << iter->first << ", killing CGI process and sending 504 response to client";
//                 int pipe_fd = iter->first;
//                 int client_fd = _cgi_pipes[pipe_fd];

//                 kill(_cgi_pids[pipe_fd], SIGKILL);
//                 waitpid(_cgi_pids[pipe_fd], NULL, 0);
// 				DEBUG("Multiplexer") << "Killed CGI process with pid: " << _cgi_pids[pipe_fd] << " for pipe fd: " << pipe_fd;
//                 close(pipe_fd);
//                 _cgi_pipes.erase(pipe_fd);
//                 _cgi_pids.erase(pipe_fd);
//                 cgi_timeouts.erase(iter++);

//                 for (size_t i = 0; i < _pollfds.size(); i++)
//                 {
//                     if (_pollfds[i].fd == pipe_fd)
//                     {
// 						DEBUG("Multiplexer") << "Removing pipe fd: " << pipe_fd << " from pollfds due to timeout";
//                         _pollfds.erase(_pollfds.begin() + i);
//                         break;
//                     }
//                 }

//                 _clients[client_fd].response = "HTTP/1.1 504 Gateway Timeout\r\nContent-Length: 0\r\n\r\n";
//                 enableWrite(client_fd);

//                 // close(pipe_fd);
//                 // _cgi_pipes.erase(pipe_fd);
//                 // _cgi_pids.erase(pipe_fd);
//                 // cgi_timeouts.erase(iter++);

//                 // for (size_t i = 0; i < _pollfds.size(); i++)
//                 // {
//                 //     if (_pollfds[i].fd == pipe_fd)
//                 //     {
// 				// 		DEBUG("Multiplexer") << "Removing pipe fd: " << pipe_fd << " from pollfds due to timeout";
//                 //         _pollfds.erase(_pollfds.begin() + i);
//                 //         break;
//                 //     }
//                 // }
//             }
//             else 
//                 iter++;
//         }
// 		DEBUG("Multiplexer") << "Finished checking CGI timeouts, proceeding to poll for events";
//         int poll_ret = poll(_pollfds.data(), _pollfds.size(), -1);
//         if (poll_ret < 0)
//         {
//             ERR() << "Multiplexer::run: poll failed: " << strerror(errno);
//             if (errno == EINTR)
//                 break;
//             throw Error::Poll();
//         }
// 		DEBUG("Multiplexer") << "Poll returned with " << poll_ret << " events, processing events...";
//         for (size_t i = 0; i < _pollfds.size(); i++)
//         {
// 			DEBUG("Multiplexer") << "Processing event for fd: " << _pollfds[i].fd << ", revents: " << _pollfds[i].revents;
//             try
//             {
// 				DEBUG("Multiplexer") << "Processing event for fd: " << _pollfds[i].fd << ", revents: " << _pollfds[i].revents;
//                 bool is_server = false;
//                 if (_pollfds[i].revents == 0) {
// 					DEBUG("Multiplexer") << "No events for fd: " << _pollfds[i].fd << ", continuing to next fd";
//                     continue;
// 				}
//                 else
//                 {
// 					DEBUG("Multiplexer") << "Event detected for fd: " << _pollfds[i].fd << ", revents: " << _pollfds[i].revents;
//                     size_t j = 0;
//                     while (j < _servers.size())
//                     {
// 						DEBUG("Multiplexer") << "Checking if fd: " << _pollfds[i].fd << " is a server socket (server index: " << j << ")";
//                         if (_servers[j]->get_fd() == _pollfds[i].fd && _pollfds[i].revents & POLLIN)
//                         {
//                             _acceptNewClient(_servers[j]);
//                             is_server = true;
//                             break;
//                         }
//                         j++;
//                     }
//                     if (is_server) {
// 						DEBUG("Multiplexer") << "Accepted new client on server socket fd: " << _pollfds[i].fd << ", continuing to next fd";
//                         continue;
// 					}
//                     if (_cgi_pipes.find(_pollfds[i].fd) != _cgi_pipes.end())
//                     {
// 						DEBUG("Multiplexer") << "Handling CGI output for pipe fd: " << _pollfds[i].fd;
//                         if (_pollfds[i].revents & POLLIN)
//                         {
//                             char buffer[4096];
//                             int n = read(_pollfds[i].fd, buffer, sizeof(buffer));
//                             if (n > 0)
//                                 _clients[_cgi_pipes[_pollfds[i].fd]].response.append(buffer, n);
//                             if (n == 0 || n == -1)
//                             {
//                                 int client_fd = _cgi_pipes[_pollfds[i].fd];
//                                 DEBUG("Multiplexer") << "Handling CGI output for pipe fd: " << _pollfds[i].fd;
//                                 enableWrite(client_fd);
//                                 waitpid(_cgi_pids[_pollfds[i].fd], NULL, 0);
//                                 DEBUG("Multiplexer") << "Killed CGI process with pid: " << _cgi_pids[_pollfds[i].fd] << " for pipe fd: " << _pollfds[i].fd;
//                                 close(_pollfds[i].fd);
//                                 _pollfds.erase(_pollfds.begin() + i);
//                                 _cgi_pids.erase(_pollfds[i].fd);
//                                 _cgi_pipes.erase(_pollfds[i].fd);
//                                 continue;
//                             }
//                         }
//                     }
//                     if (_pollfds[i].revents & POLLIN) 
// 					{
// 						DEBUG("Multiplexer") << "run Reading from client fd: " << _pollfds[i].fd;
//                         _readClient(_pollfds[i].fd);
// 					}
//                     if (_pollfds[i].revents & POLLOUT)
// 					{
//                         DEBUG("Multiplexer") << "run Writing to client fd: " << _pollfds[i].fd;
//                         _writeClient(_pollfds[i].fd);
// 					}
//                     if (_pollfds[i].revents & (POLLHUP | POLLERR))
// 					{
//                         DEBUG("Multiplexer") << "run Handling POLLHUP or POLLERR for fd: " << _pollfds[i].fd;
//                         _removeClient(_pollfds[i].fd);
// 					}
// 					DEBUG("Multiplexer") << "run Finished processing event for fd: " << _pollfds[i].fd << ", moving to next fd";
//                 }
//             }
//             catch(const std::exception& e)
//             {
// 				DEBUG("Multiplexer") << "Exception occurred while processing event for fd: " << _pollfds[i].fd << ", error: " << e.what();
//                 std::cerr << e.what() << '\n';
//             }
//         }
// 		DDEBUG("Multiplexer") << "run: finished processing events, waiting for next poll";
//     }
// }


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
    DEBUG("Multiplexer") << "_removeClient: closed client fd=" << fd;
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
        WARN() << "Multiplexer::handlePeerShutdown: truncated request, responding status=400 fd=" << fd;
        request.setStatusCode(400);
        request.state = ClientRequest::ERROR_STATE;
    }
    enableWrite(fd);
}


void Multiplexer::_readClient(int fd)
{
    char buffer[4096];
    std::map<int, Client>::iterator iter = _clients.find(fd);
	DDEBUG("Multiplexer") << "_readClient: reading from client fd=" << fd;
    if (iter != _clients.end())
    {
        int bytesRead = recv(fd, buffer, sizeof(buffer), 0);
		DDEBUG("Multiplexer") << "_readClient: read " << bytesRead << " bytes from client fd=" << fd;
        if (bytesRead == -1)
        {
            WARN() << "Multiplexer::_readClient: recv failed fd=" << fd << ": " << strerror(errno);
            _removeClient(fd);
            return;
        }
        else if (bytesRead > 0)
        {
            iter->second.request.append(buffer, bytesRead);
            DDEBUG("Multiplexer") << "_readClient: appended " << bytesRead << " bytes to request buffer fd=" << fd;
            // DDEBUG("Multiplexer") << "\n" <<buffer << "\n";
            iter->second.parsed_request.parse(iter->second);
            if (iter->second.parsed_request.state == ClientRequest::BODY)
			{
				DDEBUG("Multiplexer") << "_readClient: request state is BODY, calling BodyRequest fd=" << fd;
                iter->second.parsed_request.BodyRequest(iter->second);
			}

        }
        else if (bytesRead == 0)
        {
            DEBUG("Multiplexer") << "_readClient: peer closed fd=" << fd;
            handlePeerShutdown(fd, iter->second);
            return;
        }
        if (iter->second.parsed_request.state == ClientRequest::DONE || iter->second.parsed_request.state == ClientRequest::ERROR_STATE)
            enableWrite(fd);
    }
}

///////////////////////////////////////////////// 10: poll loop index invalidation //////////////////////////////////////////


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

    INFO() << "Multiplexer::run: running, waiting for events";
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
            ERR() << "Multiplexer::run: epoll_wait failed: " << strerror(errno);
            throw Error::Epoll();
        }
        if (ready == 0)
            continue;
        DDEBUG("Multiplexer") << "run: epoll reported " << ready << " ready fd(s)";
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



///////////////////////////////////////////////// 23: connection always closed ////////////////////////////////////////////////////




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
    {
        WARN() << "Multiplexer::setEvents: epoll_ctl MOD failed fd=" << fd
               << ": " << strerror(errno);
        return;
    }
    it->second = events;
}

    // finishResponse logic removed


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
    DEBUG("Multiplexer") << "evictOldestClient: closing least recently used fd=" << oldest
                         << " live clients=" << _clients.size();
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
    {
        DEBUG("Multiplexer") << "closeIdleClients: idle timeout fd=" << expired[i];
        _removeClient(expired[i]);
    }
}





