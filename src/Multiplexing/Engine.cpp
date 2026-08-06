#include "../../includes/multiplexing/header.hpp"
#include "../../includes/Request/ClientRequest.hpp"
// #include "../../includes/Request/RequestHelpers.hpp"
#include "../../includes/Response/Dispatcher.hpp"

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

Multiplexer::~Multiplexer()
{
    size_t i = 0;
    while (i < _servers.size())
    {
        delete _servers[i];
        i++;
    }
}

Server_block& which_server(int port)
{
    size_t i = 0;
    while (i < Conf_File::Servers.size())
    {
        if (Conf_File::Servers[i].listen_port == port)
            return Conf_File::Servers[i];
    }
    return Conf_File::Servers[0];
}

// std::string& Multiplexer::_fill_cgi_response(int fd)
// {
//     char buffer[4096];
//     std::string content;
//     ssize_t bytes_read;
//     while ((bytes_read = read(fd, buffer, sizeof(buffer))) > 0)
//     {
//         content.append(buffer, static_cast<std::size_t>(bytes_read));
//     }
//     return content;
// }

void    Multiplexer::prepareResponse(Client &client)
{
    if (client.response_prepared)
        return;
    Response response = Dispatcher::dispatch(client, which_server(client.port));
    // if (client.cgi_started)
    // {
    //     int client_fd = handleClient(client.fd);
    //     client.response = _fill_cgi_response(client_fd);
    //     client.response_prepared = true;
    //     return;
    // }
    client.response = response.toString();
    client.response_prepared = true;
}


bool    Multiplexer::sendResponse(int fd, Client &client)
{
    if (client.response.empty())
        return true;
    ssize_t sent = send( fd,client.response.c_str(),client.response.size(),MSG_NOSIGNAL);
    if (sent <= 0)
        return false;
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

void    Multiplexer::disableWrite(int fd)
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

void    Multiplexer::_writeClient(int fd)
{
    std::map<int, Client>::iterator it = _clients.find(fd);
    if (it == _clients.end())
        return;
    Client &client = it->second;
    prepareResponse(client);
    if (!sendResponse(fd, client))
        return;
    if (client.stream_file_fd != -1)
    {
        sendStreaming(fd, client);
        return;
    }
	client.reset();
    client.response_prepared = false;
    disableWrite(fd);
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

int Socket::get_listen_port()
{
    return _port;
}

// int Socket::acceptClient()
// {
//     struct sockaddr_in client_addr = {};
//     socklen_t len  = sizeof(client_addr);
//     int client_fd = accept(fd, (struct sockaddr *)&client_addr, &len);
//     if (client_fd == -1)
//         throw Error::Accept();
//     return client_fd;
// }


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

// std::string Multiplexer::_generateClientID(int fd)
// {
//     std::string id;
//     srand(time(NULL) ^ fd);
//     std::string valid_chars = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
//     for (size_t i =0; i < 32; i++)
//     {
//         ssize_t result = rand() % 62;
//         id.push_back(valid_chars[result]);
//     }
//     return id;
// }

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
    // client.client_id = client_uniq_id;
    client.port = s->get_listen_port();
    // client.session_id = _generateClientID(client.fd);
    client.parsed_request.state = ClientRequest::HEADERS;
    // client.search_offset = 0;
    // client.bytes_received = 0;
    // client.content_length = 0;
    _clients[client_fd] = client;

    struct pollfd pfd;
    pfd.fd = client_fd;
    pfd.events = POLLIN;
    pfd.revents = 0;
    _pollfds.push_back(pfd);
    // client_uniq_id++;
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
            cgi_timeouts[pipe_fd] = time(NULL);
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
        std::map<int , time_t>::iterator iter = cgi_timeouts.begin();
        time_t current = time(NULL);
        while (iter != cgi_timeouts.end())
        {
            if ((current - iter->second) > 5)
            {
                int pipe_fd = iter->first;
                int client_fd = _cgi_pipes[iter->first];
                kill(_cgi_pids[pipe_fd], SIGKILL);
                waitpid(_cgi_pids[pipe_fd], NULL, 0);
                _clients[_cgi_pipes[pipe_fd]].response = "HTTP/1.1 504 Gateway Timeout\r\nContent-Length: 0\r\n\r\n";
                enableWrite(_cgi_pipes[pipe_fd]);

                close(pipe_fd);
                _cgi_pipes.erase(pipe_fd);
                _cgi_pids.erase(pipe_fd);
                cgi_timeouts.erase(iter++);

                for (size_t i = 0; i < _pollfds.size(); i++)
                {
                    if (_pollfds[i].fd == client_fd)
                    {
                        _pollfds.erase(_pollfds.begin() + i);
                        break;
                    }
                }
            }
            else 
                iter++;
        }
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
                                _pollfds.erase(_pollfds.begin() + i);
                                _cgi_pids.erase(_pollfds[i].fd);
                                _cgi_pipes.erase(_pollfds[i].fd);
                                continue;
                            }
                        }
                    }
                    // std::cout << _pollfds[i].revents << "\n";

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


bool Response::isCGI() const
{
    return _mode == CGI_RESPONSE;
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
            iter->second.parsed_request.state = ClientRequest::DONE;
            enableWrite(fd);
            return;
        }
        if (iter->second.parsed_request.state == ClientRequest::DONE || iter->second.parsed_request.state == ClientRequest::ERROR_STATE)
            enableWrite(fd);
    }
    // enableWrite(fd);
}


// void Multiplexer::_writeClient(int fd)
// {
//     char buffer[4096];

//     std::map<int, Client>::iterator iter = _clients.find(fd);
//     if (iter == _clients.end())
//         return;

//     Client &client = iter->second;
//     // Prepare response only once
//     if (client.response_prepared == false)
//     {
//         Response parsed_response = Dispatcher::dispatch(client, which_server(client.port));
//         client.response = parsed_response.toString();
//         client.response_prepared = true;

//         if (parsed_response.isCGI())
//         {
//             int cgi_fd = handleClient(fd);
//             if (cgi_fd == ERROR)
//             {
//                 _removeClient(fd);
//                 return;
//             }

//             client.cgi_started = true;
//             return;
//         }
//     }
//     // Send normal response
//     int n = send(fd,client.response.c_str(),client.response.size(),MSG_NOSIGNAL);
//     if (n <= 0)
//     {
//         _removeClient(fd);
//         return;
//     }
//     client.response.erase(0, n);
//     if (!client.response.empty())
//         return;
//     if (client.stream_file_fd != -1)
//     {
//         ssize_t bytesRead =
//             read(client.stream_file_fd, buffer, sizeof(buffer));

//         if (bytesRead > 0)
//             send(fd, buffer, bytesRead, MSG_NOSIGNAL);

//         close(client.stream_file_fd);
//         client.stream_file_fd = -1;
//         client.stream_bytes_remaining = 0;
//     }
//     // Finished
//     client.response_prepared = false;
//     for (size_t i = 0; i < _pollfds.size(); i++)
//     {
//         if (_pollfds[i].fd == fd)
//         {
//             _pollfds[i].events &= ~POLLOUT;
//             break;
//         }
//     }
// }


// void Multiplexer::_writeClient(int fd)
// {
//     std::map<int, Client>::iterator it = _clients.find(fd);
//     if (it == _clients.end())
//         return;

//     Client &client = it->second;
//     // Prepare response only once
//     if (!client.response_prepared)
//     {
//         Response response = Dispatcher::dispatch(client, which_server(client.port));
//         client.response = response.toString();
//         client.response_prepared = true;
//     }
//     // Send HTTP headers / normal response
//     if (!client.response.empty())
//     {
//         ssize_t sent = send(fd,client.response.c_str(),client.response.size(),MSG_NOSIGNAL);
//         if (sent <= 0)
//         {
//             _removeClient(fd);
//             return;
//         }
//         client.response.erase(0, sent);
//         if (!client.response.empty())
//             return;
//     }
//     // Streaming
//     if (client.stream_file_fd != -1)
//     {
//         // Read a new chunk only if previous one is fully sent
//         if (client.stream_buffer_offset == client.stream_buffer_size)
//         {
//             client.stream_buffer_size = read(client.stream_file_fd,client.stream_buffer,sizeof(client.stream_buffer));
//             client.stream_buffer_offset = 0;
//             if (client.stream_buffer_size <= 0)
//             {
//                 close(client.stream_file_fd);
//                 client.stream_file_fd = -1;
//                 client.stream_bytes_remaining = 0;
//                 client.stream_buffer_size = 0;
//                 client.stream_buffer_offset = 0;
//                 client.response_prepared = false;

//                 for (size_t i = 0; i < _pollfds.size(); i++)
//                 {
//                     if (_pollfds[i].fd == fd)
//                     {
//                         _pollfds[i].events &= ~POLLOUT;
//                         break;
//                     }
//                 }
//                 return;
//             }
//         }
//         ssize_t sent = send(fd,client.stream_buffer + client.stream_buffer_offset,client.stream_buffer_size - client.stream_buffer_offset,MSG_NOSIGNAL);
//         if (sent <= 0)
//         {
//             client.stream_file_fd = -1;
//             client.stream_bytes_remaining = 0;
//             client.stream_buffer_size = 0;
//             client.stream_buffer_offset = 0;
//             client.response_prepared = false;
//             close(client.stream_file_fd);
//             _removeClient(fd);
//             return;
//         }
//         client.stream_buffer_offset += sent;
//         client.stream_bytes_remaining -= sent;

//         return;
//     }
//     // Finished normal response
//     client.response_prepared = false;

//     for (size_t i = 0; i < _pollfds.size(); i++)
//     {
//         if (_pollfds[i].fd == fd)
//         {
//             _pollfds[i].events &= ~POLLOUT;
//             break;
//         }
//     }
// }


// void Multiplexer::_writeClient(int fd)
// {
//     char buffer[4096];
//     std::map<int, Client>::iterator iter = _clients.find(fd);
//     if (iter == _clients.end())
//         return;
//     Response parsed_response =  Dispatcher::dispatch(iter->second, which_server(iter->second.port));
//     iter->second.response = parsed_response.toString();
//     int n = send(fd,iter->second.response.c_str(),iter->second.response.size(),MSG_NOSIGNAL);
//     if (!parsed_response.isStreaming())
//     {
//         iter->second.response = parsed_response.toString();
//         send(fd,iter->second.response.c_str(),iter->second.response.size(),MSG_NOSIGNAL);
//     }
//     else
//     {
//         while(iter->second.stream_bytes_remaining > 0)
//         {
//             ssize_t bytesRead = read(iter->second.stream_file_fd, buffer, sizeof(buffer));
//             if (bytesRead <= 0)
//                 break;
//             send(iter->second.fd, buffer, bytesRead, MSG_NOSIGNAL);
//             iter->second.stream_bytes_remaining -= bytesRead;
//         }
//     }
//     // if (!parsed_response.isStreaming())
//     // {
//     //     iter->second.response = parsed_response.toString();
//     //     send(fd,iter->second.response.c_str(),iter->second.response.size(),MSG_NOSIGNAL);
//     // }
//     // if (parsed_response.isStreaming())
//     // {
//     //     read(iter->second.stream_file_fd, buffer, sizeof(buffer));
//     //     send(iter->second.fd, buffer, sizeof(buffer), MSG_NOSIGNAL);
//     //     close(iter->second.stream_file_fd);
//     //     iter->second.stream_file_fd = -1;
//     //     iter->second.stream_bytes_remaining = 0;
//     // }
//     // parse_request();
//     // iter->second.response = "HTTP/1.1 200 OK\r\nContent-Length: 5\r\n\r\nHello";
//     // iter->second.parsed_request.getRequestPath();
//     // int n = send(fd, iter->second.response.c_str(), iter->second.response.size(), MSG_NOSIGNAL);
//     if (n <= -1)
//     {
//         _removeClient(fd);
//         return ;
//     }
//     iter->second.response.erase(0, n);
//     if (iter->second.response.empty())
//     {
//         for (size_t i = 0; i < _pollfds.size(); i++)
//         {
//             if (_pollfds[i].fd == fd)
//             {
//                 _pollfds[i].events &= ~POLLOUT;
//                 break;
//             }
//         }
//     }
// }



// int which_status_code(int status_code)
// {
//     if (status_code == 200)
//         return HTTP_200_OK;
//     else if (status_code == 201)
//         return HTTP_201_CREATED;
//     else if (status_code == 204)
//         return HTTP_204_NO_CONTENT;
//     else if (status_code == 301)
//         return HTTP_301_MOVED_PERMANENTLY;
//     else if (status_code == 302) 
//         return HTTP_302_FOUND;
//     else if (status_code == 304) 
//         return HTTP_304_NOT_MODIFIED;
//     else if (status_code == 400) 
//         return HTTP_400_BAD_REQUEST;
//     else if (status_code == 403) 
//         return HTTP_403_FORBIDDEN;
//     else if (status_code == 404) 
//         return HTTP_404_NOT_FOUND;
//     else if (status_code == 405) 
//         return HTTP_405_METHOD_NOT_ALLOWED;
//     else if (status_code == 408) 
//         return HTTP_408_REQUEST_TIMEOUT;
//     else if (status_code == 409) 
//         return HTTP_409_CONFLICT;
//     else if (status_code == 410) 
//         return HTTP_410_GONE;
//     else if (status_code == 411) 
//         return HTTP_411_LENGTH_REQUIRED;
//     else if (status_code == 413) 
//         return HTTP_413_PAYLOAD_TOO_LARGE;
//     else if (status_code == 414) 
//         return HTTP_414_URI_TOO_LONG;
//     else if (status_code == 415) 
//         return HTTP_415_UNSUPPORTED_MEDIA;   
//     else if (status_code == 500) 
//         return HTTP_500_INTERNAL_SERVER_ERROR;
//     else if (status_code == 502) 
//         return HTTP_502_BAD_GATEWAY;
//     else if (status_code == 504) 
//         return HTTP_504_GATEWAY_TIMEOUT;
//     else if (status_code == 505) 
//         return HTTP_505_HTTP_VERSION_NOT_SUPPORTED;
//     else                         
//         return HTTP_500_INTERNAL_SERVER_ERROR;
// }

// void Multiplexer::readCGI(int fd)
// {
//     char buffer[4096];

//     ssize_t n = read(fd, buffer, sizeof(buffer));

//     int client_fd = _cgi_pipes[fd];
//     Client &client = _clients[client_fd];

//     if (n > 0)
//     {
//         client.response.append(buffer, n);
//         return;
//     }
//     close(fd);
//     _cgi_pipes.erase(fd);

//     client.cgi_started = false;

//     enableWrite(client_fd);
// }


