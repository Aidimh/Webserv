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
    client.state = READING_HEADERS;
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


std::string get_listen_value(std::string& host)
{
    size_t pos = host.find(":");
    if (pos != std::string::npos)
        return host.substr(pos + 1);
    else
        return NULL;
}



int Multiplexer::handleClient(int fd)
{
    // Step 1: find matching server block
    size_t server_index = 0;
    for (size_t i = 0; i < Conf_File::Servers.size(); i++)
    {
        if (get_listen_value(_clients[fd].parsed_request.headers["Host"]) == Conf_File::Servers[i].listen_port_str)
        {
            server_index = i;
            break;
        }
    }

    // Step 2: find matching location block
    size_t location_index = 0;
    size_t longest_match = 0;
    for (size_t i = 0; i < Conf_File::Servers[server_index].location.size(); i++)
    {
        std::string loc_path = Conf_File::Servers[server_index].location[i].path;
        if (_clients[fd].parsed_request.method_path.find(loc_path) == 0 && loc_path.size() > longest_match)
        {
            longest_match = loc_path.size();
            location_index = i;
        }
    }

    Location_Config& loc = Conf_File::Servers[server_index].location[location_index];

    // Step 3: check CGI extension
    std::string req_path = _clients[fd].parsed_request.method_path;
    size_t dot_pos = req_path.rfind('.');
    if (dot_pos == std::string::npos)
        return 0; // no extension, not CGI

    std::string extension = req_path.substr(dot_pos);
    for (size_t i = 0; i < loc.cgi_extensions.size(); i++)
    {
        if (loc.cgi_extensions[i] == extension)
        {
            // Step 4: CGI match, create and execute
            CGI cgi(_clients[fd], loc);
            cgi.execute();
            return 1;
        }
    }
    return 0; // not CGI, teammate handles static file
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

void Multiplexer::_readClient(int fd)
{
    char buffer[4096];
    std::map<int, Client>::iterator iter = _clients.find(fd);
    if (iter != _clients.end())
    {
        int n = recv(fd, buffer, sizeof(buffer), 0);
        if (n == 0)
        {
            // iter->second.state = CLOSING;
            std::cout << "Client Disconnected\n";
            _removeClient(fd);
        }
        else if (n == -1)
            _removeClient(fd);
        else
            iter->second.request.append(buffer, n);
    }
    std::cout << "client reading\n";
    std::cout << "request from client " << fd << ":\n" << iter->second.request << std::endl;
    enableWrite(fd);
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
    _clients[fd].state = WRITING_RESPONSE;
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