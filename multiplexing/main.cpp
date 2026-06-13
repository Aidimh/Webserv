
#include "header.hpp"
#include "Error.hpp"

bool loop_is_true = true;
int server_index = 0;


std::vector<Server_block> Conf_File::Servers;
std::vector<std::string> Conf_File::tokens;

// int main(int ac, char **av)
// {
//     char buffer[409600];
//     int sock_fd = socket(AF_INET, SOCK_STREAM, 0);
//     if (sock_fd < 0)
//     {
//         perror("Error!\n");
//         std::cout << "Socket creation failed !!\n";
//         return ERROR;
//     }
//     struct sockaddr_in addr;
//     memset(&addr, 0, sizeof(sockaddr_in));
//     addr.sin_family = AF_INET;
//     addr.sin_port = htons(4050);
//     addr.sin_addr.s_addr = INADDR_ANY;
//     bind(sock_fd, (struct sockaddr *)&addr, sizeof(addr));
//     listen(sock_fd, 10);
//     int new_sock = accept(sock_fd, NULL, NULL);
//     recv(new_sock, buffer, 409600, 0);
//     std::cout << buffer;
//     std::cout << "reached here\n";
// }


                            // std::cout << "this is the request fd " << fds[i].fd << "\n";
                            // std::cout << "this is the revents values " << fds[i].revents << "\n";
                            // std::cout << "this is the POLLIN value " << POLLIN << "\n";
                            // exit(1);
                            // Add the new guest to our list
                                // std::cout << "Request received:\n" << buffer << "\n";
                                
                                // IMPORTANT: Send a response so the browser doesn't hang!
void open_file(std::string filename)
{
    std::ifstream file(filename.c_str());
    if (!file.is_open())
        throw Error::FileNotFound();

    std::string content((std::istreambuf_iterator<char>(file)),
                         std::istreambuf_iterator<char>());
    file.close();
    size_t i = 0;
    // std::cout << content << "\n";
    // exit(1);

    while (i < content.size())
    {
        if (isspace(content[i]))
        {
            i++;
            continue;
        }
        if (content[i] == '#')
        {
            while (i < content.size() && content[i] != '\n')
                i++;
            continue;
        }
        if (content[i] == '{' || content[i] == '}' || content[i] == ';')
        {
            Conf_File::tokens.push_back(std::string(1, content[i]));
            i++;
            continue;
        }
        std::string word;
        while (i < content.size()
                && !isspace(content[i])
                && content[i] != '{'
                && content[i] != '}'
                && content[i] != ';'
                && content[i] != '#')
        {
            word += content[i];
            i++;
        }
        Conf_File::tokens.push_back(word);
    }

    if (Conf_File::tokens.empty())
        throw Error::EmptyConfig();
}

int main(int ac , char **av)
{
    signal(SIGINT, handle_sigint);
    signal(SIGQUIT, handle_sigquit);
    signal(SIGTSTP, handle_sigstp);
    try
    {
        if (ac != 2)
            throw Error::Argc();
        if (!av || av[1][0] == '\0')
            throw Error::Argv();
        int fd_sock = socket(AF_INET, SOCK_STREAM, 0);
        if (fd_sock < 0)
            throw Error::Socket();
        open_file(av[1]);
        validate_file();
        std::cout << "reached\n";
        parse_config_file();
        // std::cout << Conf_File::tokens.size() << std::endl;
        // size_t i = 0;
        // while (i < Conf_File::tokens.size())
        // {
        //     std::cout << Conf_File::tokens[i] << "\n";
        //     i++;
        // }
        // exit(1);
        // Conf_File::reset_flags();
        // parse_file();
        // while (i < Conf_File::tokens.size())
        // {
        //     std::cout << Conf_File::tokens[i] << "\n";
        //     i++;
        // }
        // while (i < Conf_File::rawLines.size())
        // {
        //     std::cout << Conf_File::rawLines[i] << std::endl;
        //     i++;
        // }
        exit(1);
        int opt = 1;
        setsockopt(fd_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
        struct sockaddr_in addr;
        memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_port = htons(4050);
        addr.sin_addr.s_addr = INADDR_ANY;
        if (bind(fd_sock, (struct sockaddr *)&addr, sizeof(addr)) < 0)
            throw Error::Bind();
        if (listen(fd_sock, 10) < 0)
            throw Error::Listen();

        std::vector<struct Client> Client;
        std::vector<struct pollfd> fds;
        std::vector<struct pollfd> tmp_fds;
        struct pollfd listen_pfd = {fd_sock, POLLIN, 0};
        fds.push_back(listen_pfd);
        std::cout << "Webserv running on port 4050..." << std::endl;
        while (loop_is_true)
        {
            int poll_ret = poll(fds.data(), fds.size(), -1);
            // if (!loop_is_true)
            //         break;
            if (poll_ret < 0)
            {
                if (errno == EINTR)
                    break;
                throw Error::Poll();
            }
            for (size_t i = 0; i < fds.size(); i++)
            {
                try
                {
                    struct Client client;
                    if (fds[i].revents & POLLIN)
                    {
                        if (fds[i].fd == fd_sock)
                        {
                            struct sockaddr_in client_id;
                            client.fd = fds[i].fd;
                            socklen_t size = sizeof(client_id);
                            int new_sock = accept(fd_sock, (struct sockaddr *)&client_id, &size);
                            if (new_sock < 0)
                                throw Error::Accept();
                            struct pollfd new_pfd = {new_sock, POLLIN, 0};
                            tmp_fds.push_back(new_pfd);
                            std::cout << "New client Added: " << inet_ntoa(client_id.sin_addr) << "\n";
                        } 
                        else
                        {
                            client.state = PROCESSING;
                            char buffer[4096] = {0};
                            int new_bytes_received = recv(fds[i].fd, buffer, 4096, 0);
                            if (new_bytes_received == 0)
                            {
                                client.state = CLOSING;
                                std::cout << "Client Disconnected\n";
                                close(fds[i].fd);
                                fds.erase(fds.begin() + i--);
                            }
                            else if (new_bytes_received < 0)
                            {
                                close(fds[i].fd);
                                fds.erase(fds.begin() + i--);
                                throw Error::Recv();
                            }
                            client.state = READING_HEADERS;
                            client.bytes_received += new_bytes_received;
                            client.request.append(buffer, new_bytes_received);
                            size_t pos = client.request.find("\r\n\r\n", client.search_offset);
                            if (pos != std::string::npos)
                            {
                                client.state = READING_BODY;
                                std::cout << "Content starts here\n";
                                std::cout << client.request;
                            }
                            client.search_offset = ((client.search_offset + new_bytes_received) - 3);
                            // std::cout << buffer ;
                            // if (new_bytes_received > 0)
                            //     client.state = READING_HEADERS;
                            client.state = WRITING_RESPONSE;
                            std::string res = "HTTP/1.1 200 OK\r\nContent-Length: 11\r\n\r\nHello World";
                            ssize_t sent = send(fds[i].fd, res.c_str(), res.length(), 0);
                            if (sent < 0)
                            {
                                close(fds[i].fd);
                                fds.erase(fds.begin() + i--);
                                throw Error::Send();
                            }
                            Client.push_back(client);
                            std::cout << "----------- I'm here ----------\n";
                            // std::cout << buffer;
                        }
                    }
                }
                catch(const std::exception& e)
                {
                    std::cerr << e.what() << '\n';
                }
        }
        fds.insert(fds.end(), tmp_fds.begin(), tmp_fds.end());
        tmp_fds.clear();
    }
    if (Client[0].request.c_str())
        std::cout << Client[0].request;
    for (size_t i = 0; i < fds.size(); i++)
        close(fds[i].fd);
    close(fd_sock);
    }
    catch(const std::exception& e)
    {
        std::cerr << e.what() << '\n';
    }
    return 0;
}
