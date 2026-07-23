#include "header.hpp"

bool loop_is_true = true;



// int AFd::get_fd() const
// {
//     return fd;
// }

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

// AFd::~AFd()
// {
//     close(fd);
// }

// ---------------------------------------- Socket Class ------------------------------------- //

Socket::Socket() : _port(0)
{
    
}

void Socket::setup(int port, const std::string& host)
{
    _port = port;
    _host = host;

    fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd == -1)
        throw Error::Socket();
    int opt = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

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

int Socket::acceptClient()
{
    struct sockaddr_in client_addr = {};
    socklen_t len  = sizeof(client_addr);
    int client_fd = accept(fd, (struct sockaddr *)&client_addr, &len);
    if (client_fd == -1)
        throw Error::Accept();
    return client_fd;
}


Socket::~Socket()
{
    close(fd);
}

// std::string Socket::GetClientIp()
// { 
//     return _host;
// }


// ------------------------------------------- Multiplexer Class ------------------------------ //


Multiplexer::Multiplexer() {}

void Multiplexer::addServer(Socket *s)
{
    _servers.push_back(s);
    struct pollfd addr;
    addr.events = POLLIN;
    addr.fd = s->get_fd();
    addr.revents = 0;
    _pollfds.push_back(addr);

}

void Multiplexer::_acceptNewClient(Socket *s)
{
    struct sockaddr_in client_id;
    socklen_t len = sizeof(client_id);
    int client_fd = accept(s->get_fd(), (struct sockaddr *)&client_id, &len);
    fcntl(client_fd, F_SETFL, O_NONBLOCK);

    Client client;

    client.fd = client_fd;
    client.parsed_request.state = ClientRequest::HEADERS;
    client.search_offset = 0;
    client.bytes_received = 0;
    client.content_length = 0;
    _clients[client_fd] = client;

    struct pollfd pfd;

    pfd.fd = client_fd;
    pfd.events = POLLIN;
    pfd.revents = 0;
    _pollfds.push_back(pfd);
}


std::string get_listen_value(const std::string& host)
{
    size_t pos = host.find(":");
    if (pos != std::string::npos)
        return host.substr(pos + 1);
    else
        return "";
}

static std::string get_header_value(const std::map<std::string, std::string>& headers, const std::string& key)
{
    std::map<std::string, std::string>::const_iterator it = headers.find(key);
    if (it == headers.end())
        return "";
    return it->second;
}

int Multiplexer::handleClient(int fd)
{
    size_t server_index = 0;
    for (size_t i = 0; i < Conf_File::Servers.size(); i++)
    {
        if (get_listen_value(get_header_value(_clients[fd].parsed_request.getHeaders(), "host")) == Conf_File::Servers[i].listen_port_str)
        {
            server_index = i;
            break;
        }
    }

    size_t location_index = 0;
    size_t longest_match = 0;
    for (size_t i = 0; i < Conf_File::Servers[server_index].location.size(); i++)
    {
        std::string loc_path = Conf_File::Servers[server_index].location[i].path;
        if (_clients[fd].parsed_request.getRequestPath().find(loc_path) == 0 && loc_path.size() > longest_match)
        {
            longest_match = loc_path.size();
            location_index = i;
        }
    }
    Location_Config& loc = Conf_File::Servers[server_index].location[location_index];
    std::string req_path = _clients[fd].parsed_request.getRequestPath();
    size_t dot_pos = req_path.rfind('.');
    if (dot_pos == std::string::npos)
        return 0;

    std::string extension = req_path.substr(dot_pos);
    for (size_t i = 0; i < loc.cgi_extensions.size(); i++)
    {
        if (loc.cgi_extensions[i] == extension)
        {
            CGI cgi(_clients[fd], loc);
            int pipe_fd = cgi.execute(_cgi_pids);
            if (pipe_fd == -1)
                return ERROR;
            cgi.writeToChild();
            _cgi_pipes[pipe_fd] = fd;
            struct pollfd pfd;
            pfd.fd = pipe_fd;
            pfd.events = POLLIN;
            pfd.revents = 0;
            _pollfds.push_back(pfd);
            return 1;
        }
    }
    return 0;
}

void Multiplexer::run()
{
    while (loop_is_true)  
    {
        int poll_ret = poll(_pollfds.data(), _pollfds.size(), -1);
        if (poll_ret < 0)
        {
            if (errno == EINTR)
                break;
            throw Error::Poll();
        }
        for (size_t i = 0; i < _pollfds.size(); i++)
        {
            try
            {
                bool is_server = false;
                if (_pollfds[i].revents == 0)
                    continue;
                else
                {
                    size_t j = 0;
                    while (j < _servers.size())
                    {
                        if (_servers[j]->get_fd() == _pollfds[i].fd && _pollfds[i].revents & POLLIN)
                        {
                            _acceptNewClient(_servers[j]);
                            is_server = true;
                            break;
                        }
                        j++;
                    }
                    if (is_server)
                        continue;
                    if (_cgi_pipes.find(_pollfds[i].fd) != _cgi_pipes.end())
                    {
                        if (_pollfds[i].revents & POLLIN)
                        {
                            char buffer[4096];
                            int n = read(_pollfds[i].fd, buffer, sizeof(buffer));
                            if (n > 0)
                                _clients[_cgi_pipes[_pollfds[i].fd]].response.append(buffer, n);
                            if (n == 0 || n == -1)
                            {
                                int client_fd = _cgi_pipes[_pollfds[i].fd];
                                enableWrite(client_fd);
                                waitpid(_cgi_pids[_pollfds[i].fd], NULL, 0);
                                close(_pollfds[i].fd);
                                _cgi_pids.erase(_pollfds[i].fd);
                                _pollfds.erase(_pollfds.begin() + i);
                                _cgi_pipes.erase(_pollfds[i].fd);
                                continue;
                            }
                        }
                    }
                    std::cout << _pollfds[i].revents << "\n";

                    if (_pollfds[i].revents & POLLIN)
                        _readClient(_pollfds[i].fd);
                    if (_pollfds[i].revents & POLLOUT)
                        _writeClient(_pollfds[i].fd);
                    if (_pollfds[i].revents & (POLLHUP | POLLERR))
                        _removeClient(_pollfds[i].fd);
                }
            }
            catch(const std::exception& e)
            {
                std::cerr << e.what() << '\n';
            }
        }
    }
}


Multiplexer::~Multiplexer()
{
    size_t i = 0;
    while (i < _servers.size())
    {
        delete _servers[i];
        i++;
    }
}



// int Multiplexer::

// std::string serve_index(int fd)
// {
//     std::string full_path = Conf
// }

void Multiplexer::_writeClient(int fd)
{
    // char buffer[4096];
    std::map<int, Client>::iterator iter = _clients.find(fd);
    if (iter == _clients.end())
        return;
    // parse_request();
    iter->second.response = "HTTP/1.1 200 OK\r\nContent-Length: 5\r\n\r\nHello";
    int n = send(fd, iter->second.response.c_str(), iter->second.response.size(), MSG_NOSIGNAL);
    if (n <= -1)
    {
        _removeClient(fd);
        return ;
    }
    iter->second.response.erase(0, n);
    if (iter->second.response.empty())
    {
        for (size_t i = 0; i < _pollfds.size(); i++)
        {
            if (_pollfds[i].fd == fd)
            {
                _pollfds[i].events &= ~POLLOUT;
                break;
            }
        }
    }
}

void Multiplexer::enableWrite(int fd)
{
    size_t i = 0;
    while (i < _pollfds.size())
    {
        if (_pollfds[i].fd == fd)
        {
            _pollfds[i].events |= POLLOUT;
            break;
        }
        i++;
    }
}

void Multiplexer::_removeClient(int fd)
{
    std::map<int, Client>::iterator iter = _clients.find(fd);
    if (iter == _clients.end())
        return;
    close(fd);
    _clients.erase(iter);
    for (size_t i = 0; i < _pollfds.size(); i++)
    {
        if (_pollfds[i].fd == fd)
        {
            _pollfds.erase(_pollfds.begin() + i);
            break;
        }
    }
}

void Multiplexer::_readClient(int fd)
{
    char buffer[4096];
    std::map<int, Client>::iterator iter = _clients.find(fd);
    if (iter != _clients.end())
    {
        int bytesRead = recv(fd, buffer, sizeof(buffer), 0);
        if (bytesRead == 0)
        {
            iter->second.parsed_request.state = ClientRequest::DONE;
            std::cout << "Client Disconnected\n";
            _removeClient(fd);
        }
        else if (bytesRead == -1)
            _removeClient(fd);
        else
        {
            iter->second.request.append(buffer, bytesRead);
            iter->second.parsed_request.parse(iter->second);
            if (iter->second.parsed_request.state == ClientRequest::BODY)
            {
                if (iter->second.parsed_request.getContentLength() > 0 || iter->second.parsed_request.CheckTransferEncoding())
                {
                    if (!iter->second.parsed_request.CheckTransferEncoding() && iter->second.parsed_request.getBody().length() > iter->second.parsed_request.getContentLength())
                    {
                        //EPOLLOUT
                        
                    }
                }
            }
        }
    }
    enableWrite(fd);
}