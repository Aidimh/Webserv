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


bool Multiplexer::is_in_cgi_list(std::string& ext)
{
	//todo : remove this method because it should come from the config file directly
    return (ext == ".py" || ext == ".sh" || ext == ".pl" || ext == ".php" || ext == ".bla");
}


bool Multiplexer::is_cgi(const std::string& path)
{
    size_t pos = path.find(".");
    if (pos != std::string::npos)
    {
        std::string ext = path.substr(pos);
        if(is_in_cgi_list(ext))
            return true;
    }
    return false;
}

Server_block& which_server(int port)
{
    size_t i = 0;
    while (i < Conf_File::Servers.size())
    {
        size_t j = 0;
        while(j < Conf_File::Servers[i].ports_count)
        {
            if (Conf_File::Servers[i].listen_port[j] == port)
                return Conf_File::Servers[i];
            j++;
        }
        i++;
    }
    return Conf_File::Servers[0];
}

void    Multiplexer::prepareResponse(Client &client)
{
    if (client.response_prepared)
        return;
    Response response = Dispatcher::dispatch(client, which_server(client.port));
    client.response = response.toString();
    client.response_prepared = true;
}


bool    Multiplexer::sendResponse(int fd, Client &client)
{
    if (client.response.empty())
        return true;
    ssize_t sent = send( fd,client.response.c_str(),client.response.size(),MSG_NOSIGNAL);
    if (sent <= 0)
    {
        WARN() << "Multiplexer::sendResponse: send failed to client fd=" << fd
               << ": " << strerror(errno);
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
    if (is_cgi(it->second.parsed_request.getRequestPath()))
    {
        handleClient(fd);
        disableWrite(fd);
        _removeClient(fd);
        return;
    }
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
	_removeClient(fd);
    disableWrite(fd);
}


void Socket::setup(int port, const std::string& host)
{
    _port = port;
    _host = host;
    fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd == -1)
    {
        ERR() << "Socket::setup: socket failed host=" << host << " port=" << port
              << ": " << strerror(errno);
        throw Error::Socket();
    }
    DEBUG("Socket") << "setup: opened listening socket fd=" << fd;
    int opt = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

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
	DEBUG("Socket") << "~Socket: closed listening socket fd=" << fd;
    close(fd);
}


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
    if (client_fd == -1)
    {
        ERR() << "Multiplexer::_acceptNewClient: accept failed on listening fd=" << s->get_fd()
              << ": " << strerror(errno);
        return;
    }
	DEBUG("Multiplexer") << "_acceptNewClient: accepted client fd=" << client_fd
	                     << " port=" << s->get_listen_port();
    fcntl(client_fd, F_SETFL, O_NONBLOCK);

    Client client;

    client.fd = client_fd;
    client.port = s->get_listen_port();
    client.parsed_request.state = ClientRequest::HEADERS;
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

int Multiplexer::handleClient(int fd)
{
    size_t server_index = 0;
    for (size_t i = 0; i < Conf_File::Servers.size(); i++)
    {
        if (get_listen_value(get_header_value(_clients[fd].parsed_request.getHeaders(), "host")) == Conf_File::Servers[i].listen_port_str[0])
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
            // std::cout << "heres whats inside the pipe filled by cgi\n";
            // read_and_print_fd(pipe_fd);
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
		INFO() << "Multiplexer::run: running, waiting for events";
        std::map<int , time_t>::iterator iter = cgi_timeouts.begin();
        time_t current = time(NULL);
        while (iter != cgi_timeouts.end())
        {
			DEBUG("Multiplexer") << "Checking CGI timeout for pipe fd: " << iter->first << ", elapsed time: " << (current - iter->second) << " seconds";
            if ((current - iter->second) > 5)
            {
				DEBUG("Multiplexer") << "CGI timeout occurred for pipe fd: " << iter->first << ", killing CGI process and sending 504 response to client";
                int pipe_fd = iter->first;
                int client_fd = _cgi_pipes[pipe_fd];

                kill(_cgi_pids[pipe_fd], SIGKILL);
                waitpid(_cgi_pids[pipe_fd], NULL, 0);
				DEBUG("Multiplexer") << "Killed CGI process with pid: " << _cgi_pids[pipe_fd] << " for pipe fd: " << pipe_fd;
                close(pipe_fd);
                _cgi_pipes.erase(pipe_fd);
                _cgi_pids.erase(pipe_fd);
                cgi_timeouts.erase(iter++);

                for (size_t i = 0; i < _pollfds.size(); i++)
                {
                    if (_pollfds[i].fd == pipe_fd)
                    {
						DEBUG("Multiplexer") << "Removing pipe fd: " << pipe_fd << " from pollfds due to timeout";
                        _pollfds.erase(_pollfds.begin() + i);
                        break;
                    }
                }

                _clients[client_fd].response = "HTTP/1.1 504 Gateway Timeout\r\nContent-Length: 0\r\n\r\n";
                enableWrite(client_fd);

                // close(pipe_fd);
                // _cgi_pipes.erase(pipe_fd);
                // _cgi_pids.erase(pipe_fd);
                // cgi_timeouts.erase(iter++);

                // for (size_t i = 0; i < _pollfds.size(); i++)
                // {
                //     if (_pollfds[i].fd == pipe_fd)
                //     {
				// 		DEBUG("Multiplexer") << "Removing pipe fd: " << pipe_fd << " from pollfds due to timeout";
                //         _pollfds.erase(_pollfds.begin() + i);
                //         break;
                //     }
                // }
            }
            else 
                iter++;
        }
		DEBUG("Multiplexer") << "Finished checking CGI timeouts, proceeding to poll for events";
        int poll_ret = poll(_pollfds.data(), _pollfds.size(), -1);
        if (poll_ret < 0)
        {
            ERR() << "Multiplexer::run: poll failed: " << strerror(errno);
            if (errno == EINTR)
                break;
            throw Error::Poll();
        }
		DEBUG("Multiplexer") << "Poll returned with " << poll_ret << " events, processing events...";
        for (size_t i = 0; i < _pollfds.size(); i++)
        {
			DEBUG("Multiplexer") << "Processing event for fd: " << _pollfds[i].fd << ", revents: " << _pollfds[i].revents;
            try
            {
				DEBUG("Multiplexer") << "Processing event for fd: " << _pollfds[i].fd << ", revents: " << _pollfds[i].revents;
                bool is_server = false;
                if (_pollfds[i].revents == 0) {
					DEBUG("Multiplexer") << "No events for fd: " << _pollfds[i].fd << ", continuing to next fd";
                    continue;
				}
                else
                {
					DEBUG("Multiplexer") << "Event detected for fd: " << _pollfds[i].fd << ", revents: " << _pollfds[i].revents;
                    size_t j = 0;
                    while (j < _servers.size())
                    {
						DEBUG("Multiplexer") << "Checking if fd: " << _pollfds[i].fd << " is a server socket (server index: " << j << ")";
                        if (_servers[j]->get_fd() == _pollfds[i].fd && _pollfds[i].revents & POLLIN)
                        {
                            _acceptNewClient(_servers[j]);
                            is_server = true;
                            break;
                        }
                        j++;
                    }
                    if (is_server) {
						DEBUG("Multiplexer") << "Accepted new client on server socket fd: " << _pollfds[i].fd << ", continuing to next fd";
                        continue;
					}
                    if (_cgi_pipes.find(_pollfds[i].fd) != _cgi_pipes.end())
                    {
						DEBUG("Multiplexer") << "Handling CGI output for pipe fd: " << _pollfds[i].fd;
                        if (_pollfds[i].revents & POLLIN)
                        {
                            char buffer[4096];
                            int n = read(_pollfds[i].fd, buffer, sizeof(buffer));
                            if (n > 0)
                                _clients[_cgi_pipes[_pollfds[i].fd]].response.append(buffer, n);
                            if (n == 0 || n == -1)
                            {
                                int client_fd = _cgi_pipes[_pollfds[i].fd];
                                DEBUG("Multiplexer") << "Handling CGI output for pipe fd: " << _pollfds[i].fd;
                                enableWrite(client_fd);
                                waitpid(_cgi_pids[_pollfds[i].fd], NULL, 0);
                                DEBUG("Multiplexer") << "Killed CGI process with pid: " << _cgi_pids[_pollfds[i].fd] << " for pipe fd: " << _pollfds[i].fd;
                                close(_pollfds[i].fd);
                                _pollfds.erase(_pollfds.begin() + i);
                                _cgi_pids.erase(_pollfds[i].fd);
                                _cgi_pipes.erase(_pollfds[i].fd);
                                continue;
                            }
                        }
                    }
                    if (_pollfds[i].revents & POLLIN) 
					{
						DEBUG("Multiplexer") << "run Reading from client fd: " << _pollfds[i].fd;
                        _readClient(_pollfds[i].fd);
					}
                    if (_pollfds[i].revents & POLLOUT)
					{
                        DEBUG("Multiplexer") << "run Writing to client fd: " << _pollfds[i].fd;
                        _writeClient(_pollfds[i].fd);
					}
                    if (_pollfds[i].revents & (POLLHUP | POLLERR))
					{
                        DEBUG("Multiplexer") << "run Handling POLLHUP or POLLERR for fd: " << _pollfds[i].fd;
                        _removeClient(_pollfds[i].fd);
					}
					DEBUG("Multiplexer") << "run Finished processing event for fd: " << _pollfds[i].fd << ", moving to next fd";
                }
            }
            catch(const std::exception& e)
            {
				DEBUG("Multiplexer") << "Exception occurred while processing event for fd: " << _pollfds[i].fd << ", error: " << e.what();
                std::cerr << e.what() << '\n';
            }
        }
		DDEBUG("Multiplexer") << "run: finished processing events, waiting for next poll";
    }
}


bool Response::isCGI() const
{
    return _mode == CGI_RESPONSE;
}


void Multiplexer::enableWrite(int fd)
{
    size_t i = 0;
	DEBUG("Multiplexer") << "enableWrite: enabling write for fd=" << fd;
    while (i < _pollfds.size())
    {
        if (_pollfds[i].fd == fd)
        {
            _pollfds[i].events = POLLOUT;
            break;
        }
        i++;
    }
}

void Multiplexer::_removeClient(int fd)
{
    std::map<int, Client>::iterator iter = _clients.find(fd);
    if (iter == _clients.end()){
        return;
	}
	DEBUG("Multiplexer") << "_removeClient: closed client fd=" << fd;
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
            iter->second.parsed_request.state = ClientRequest::DONE;
            enableWrite(fd);
            return;
        }
        if (iter->second.parsed_request.state == ClientRequest::DONE || iter->second.parsed_request.state == ClientRequest::ERROR_STATE)
            enableWrite(fd);
    }
}