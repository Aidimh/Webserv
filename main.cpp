#include "Response/Response.hpp"
#include "Response/Dispatcher.hpp"
#include "Request/ClientRequest.hpp"
#include "Request/RequestHelpers.hpp"
#include "multiplexing/header.hpp"
#include "Error.hpp"
#include <iostream>
#include <unistd.h>
#include <fcntl.h>

// #include "header.hpp"
// #include "Error.hpp"

// int server_index = 0;


// std::vector<Server_block> Conf_File::Servers;
// std::vector<std::string> Conf_File::tokens;

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
// void open_file(std::string filename)
// {
//     std::ifstream file(filename.c_str());
//     if (!file.is_open())
//         throw Error::FileNotFound();

//     std::string content((std::istreambuf_iterator<char>(file)),
//                          std::istreambuf_iterator<char>());
//     file.close();
//     size_t i = 0;
//     // std::cout << content << "\n";
//     // exit(1);

//     while (i < content.size())
//     {
//         if (isspace(content[i]))
//         {
//             i++;
//             continue;
//         }
//         if (content[i] == '#')
//         {
//             while (i < content.size() && content[i] != '\n')
//                 i++;
//             continue;
//         }
//         if (content[i] == '{' || content[i] == '}' || content[i] == ';')
//         {
//             Conf_File::tokens.push_back(std::string(1, content[i]));
//             i++;
//             continue;
//         }
//         std::string word;
//         while (i < content.size()
//                 && !isspace(content[i])
//                 && content[i] != '{'
//                 && content[i] != '}'
//                 && content[i] != ';'
//                 && content[i] != '#')
//         {
//             word += content[i];
//             i++;
//         }
//         Conf_File::tokens.push_back(word);
//     }

//     if (Conf_File::tokens.empty())
//         throw Error::EmptyConfig();
// }


// void Print()
// {
    // size_t i = 0;
        // std::cout << Conf_File::Servers[0].error_pages[500] << "\n";
    // while (i < Conf_File::Servers[server_index].error_pages.size())
    // {
    //     std::cout << Conf_File::Servers[0].error_pages[i] << "\n";
    //     usleep(1111111);
    //     i++;
    // }
    // // For readability, creating a reference to your specific server block
    // const Server_block& srv = Conf_File::Servers[i];

    // std::cout << "========== SERVER BLOCK [" << i << "] ==========" << std::endl;

    // // --- Printing Booleans (Using std::boolalpha to print 'true'/'false' instead of 1/0)
    // std::cout << std::boolalpha;
    // std::cout << "server_found:           " << srv.server_found << std::endl;
    // std::cout << "host_found:             " << srv.host_found << std::endl;
    // std::cout << "location_found:         " << srv.location_found << std::endl;
    // std::cout << "root_found:             " << srv.root_found << std::endl;
    // std::cout << "server_name_found:      " << srv.server_name_found << std::endl;
    // std::cout << "listen_found:           " << srv.listen_found << std::endl;
    // std::cout << "index_found:            " << srv.index_found << std::endl;
    // std::cout << "error_page_found:       " << srv.error_page_found << std::endl;
    // std::cout << "client_max_body_found:  " << srv.client_max_body_found << std::endl;
    // std::cout << "uploadLimits:           " << srv.uploadLimits << std::endl;
    // std::cout << "body_size_is_MB:        " << srv.body_size_is_MB << std::endl;
    // std::cout << "body_size_is_KB:        " << srv.body_size_is_KB << std::endl;
    // std::cout << "body_size_is_BT:        " << srv.body_size_is_BT << std::endl;
    // std::cout << std::noboolalpha; // Reset bool representation back to 1/0 if preferred

    // // --- Printing Standard Primitives and Strings
    // std::cout << "listen_port:            " << srv.listen_port << std::endl;
    // std::cout << "host:                   \"" << srv.host << "\"" << std::endl;
    // std::cout << "server_name:            \"" << srv.server_name << "\"" << std::endl;
    // std::cout << "root:                   \"" << srv.root << "\"" << std::endl;
    // std::cout << "index_count:            " << srv.index_count << std::endl;
    // std::cout << "max_body_size:          " << srv.max_body_size << std::endl;
    // std::cout << "default_file:           \"" << srv.default_file << "\"" << std::endl;
    // std::cout << "autoindex:              \"" << srv.autoindex << "\"" << std::endl;

    // // --- Printing std::vector<std::string> index_files
    // std::cout << "index_files:            [ ";
    // for (std::vector<std::string>::const_iterator it = srv.index_files.begin(); it != srv.index_files.begin(); ++it) {
    //     std::cout << *it << " ";
    // }
    // std::cout << "]" << std::endl;

    // // --- Printing std::vector<std::string> methods
    // std::cout << "methods:                [ ";
    // for (std::vector<std::string>::const_iterator it = srv.methods.begin(); it != srv.methods.end(); ++it) {
    //     std::cout << *it << " ";
    // }
    // std::cout << "]" << std::endl;

    // // --- Printing std::map<int, std::string> error_pages
    // std::cout << "error_pages:            {" << std::endl;
    // for (std::map<int, std::string>::const_iterator it = srv.error_pages.begin(); it != srv.error_pages.end(); ++it) {
    //     std::cout << "  Code " << it->first << " -> \"" << it->second << "\"" << std::endl;
    // }
    // std::cout << "}" << std::endl;

    // // --- Location_Config block placeholder
    // std::cout << "location:               [Location_Config object loaded]" << std::endl;
    // std::cout << "==========================================" << std::endl;
// }

// void print()
// {
//     std::cout << "server size is " << Conf_File::Servers.size() << "\n";
//     size_t i = 0;
//     while (i < Conf_File::Servers.size())
//     {
//         std::cout << Conf_File::Servers[i].listen_port << std::endl;
//         std::cout << "server number : " << i << "' host" << Conf_File::Servers[i].host << " is " << std::endl;
//         i++;

//     }
// }


// int main()
// {
//     Client client;

//     client.request =
//     "GET /index.html HTTP/1.1\r\n"
//     "Host: localhost\r\n"
//     "\r\n";

//     client.parsed_request.parse(client);

//     Server_block server;

//     // fill server configuration

//     Response res =
//         Dispatcher::dispatch(client, server);

//     std::cout << res.toString() << std::endl;
//     // Client client;

//     // client.parsed_request.setMethod("GET");
//     // client.parsed_request.setRequestPath("/index.html");
//     // client.parsed_request.setVersion("HTTP/1.1");

//     // Server_block server;

//     // // 3mr server fields li kay7tajhom GET

//     // GET get;
//     // Response res = get.execute(client, server);

//     // std::cout << res.toString();
//     // Client client;

//     // // Fake HTTP request
//     // client.request =
//     //     "GET /index.html HTTP/1.1\r\n"
//     //     "Host: localhost\r\n"
//     //     "\r\n";

//     // // Parse request
//     // client.parsed_request.parse(client);

//     // // Fake server configuration
//     // Server_block server;

//     // server.root = "./www";
//     // server.index_files.push_back("index.html");
//     // server.autoindex = "off";

//     // // Dispatch
//     // Response response =
//     //     Dispatcher::dispatch(client, server);

//     // std::cout << "========== HTTP RESPONSE ==========\n";
//     // std::cout << response.toString() << std::endl;

//     return 0;
// }

// int main(int ac , char **av, char **envp)  
// {
//     signal(SIGINT, handle_sigint);
//     signal(SIGQUIT, handle_sigquit);
//     signal(SIGTSTP, handle_sigstp);
//     try
//     {
//         if (ac != 2)
//             throw Error::Argc();
//         if (!av || av[1][0] == '\0')
//             throw Error::Argv();
//         open_file(av[1]);
//         validate_file();
//         parse_config_file();
//         // std::cout << Conf_File::Servers[0].error_pages[404];
//         // exit(1);
//         // print();
//         // exit(1);
//         // Print();
//         // // Print();
//         // exit(1);
//         size_t i = 0;
//         Multiplexer Mux;
//         while(i < Conf_File::Servers.size())
//         {
//             Socket *s = new Socket();
//             Mux.env = envp;
//             s->setup(Conf_File::Servers[i].listen_port, Conf_File::Servers[i].host);
//             Mux.addServer(s);
//             i++;
//         }
//         Mux.run();
//         // std::cout << "after run\n";
//     }
//     catch(const std::exception& e)
//     {
//         std::cerr << e.what() << '\n';
//     }
//     return 0;
// }

        // int fd_sock = socket(AF_INET, SOCK_STREAM, 0);
        // if (fd_sock < 0)
        //     throw Error::Socket();
    //     // while(loop_is_true)
    //     // {

    //     // }
    //     // std::cout << Conf_File::tokens.size() << std::endl;
    //     // size_t i = 0;
    //     // while (i < Conf_File::tokens.size())
    //     // {
    //     //     std::cout << Conf_File::tokens[i] << "\n";
    //     //     i++;
    //     // }
    //     // exit(1);
    //     // Conf_File::reset_flags();
    //     // parse_file();
    //     // while (i < Conf_File::tokens.size())
    //     // {
    //     //     std::cout << Conf_File::tokens[i] << "\n";
    //     //     i++;
    //     // }
    //     // while (i < Conf_File::rawLines.size())
    //     // {
    //     //     std::cout << Conf_File::rawLines[i] << std::endl;
    //     //     i++;
    //     // }
    //     std::vector<struct Client> Client;
    //     // setup_Clients();
    //     exit(1);
    //     int opt = 1;
    //     setsockopt(fd_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    //     struct sockaddr_in addr;
    //     memset(&addr, 0, sizeof(addr));
    //     addr.sin_family = AF_INET;
    //     addr.sin_port = htons(4050);
    //     addr.sin_addr.s_addr = INADDR_ANY;
    //     if (bind(fd_sock, (struct sockaddr *)&addr, sizeof(addr)) < 0)
    //         throw Error::Bind();
    //     if (listen(fd_sock, 10) < 0)
    //         throw Error::Listen();

    //     std::vector<struct pollfd> fds;
    //     std::vector<struct pollfd> tmp_fds;
    //     struct pollfd listen_pfd = {fd_sock, POLLIN, 0};
    //     fds.push_back(listen_pfd);
    //     std::cout << "Webserv running on port 4050..." << std::endl;
    //     while (loop_is_true)
    //     {
    //         int poll_ret = poll(fds.data(), fds.size(), -1);
    //         // if (!loop_is_true)
    //         //         break;
    //         if (poll_ret < 0)
    //         {
    //             if (errno == EINTR)
    //                 break;
    //             throw Error::Poll();
    //         }
    //         for (size_t i = 0; i < fds.size(); i++) 
    //         {
    //             try
    //             {
    //                 struct Client client;
    //                 if (fds[i].revents & POLLIN)
    //                 {
    //                     if (fds[i].fd == fd_sock)
    //                     {
    //                         struct sockaddr_in client_id;
    //                         client.fd = fds[i].fd;
    //                         socklen_t size = sizeof(client_id);
    //                         int new_sock = accept(fd_sock, (struct sockaddr *)&client_id, &size);
    //                         if (new_sock < 0)
    //                             throw Error::Accept();
    //                         struct pollfd new_pfd = {new_sock, POLLIN, 0};
    //                         tmp_fds.push_back(new_pfd);
    //                         std::cout << "New client Added: " << inet_ntoa(client_id.sin_addr) << "\n";
    //                     } 
    //                     else
    //                     {
    //                         client.state = PROCESSING;
    //                         char buffer[4096] = {0};
    //                         int new_bytes_received = recv(fds[i].fd, buffer, 4096, 0);
    //                         if (new_bytes_received == 0)
    //                         {
    //                             client.state = CLOSING;
    //                             std::cout << "Client Disconnected\n";
    //                             close(fds[i].fd);
    //                             fds.erase(fds.begin() + i--);
    //                         }
    //                         else if (new_bytes_received < 0)
    //                         {
    //                             close(fds[i].fd);
    //                             fds.erase(fds.begin() + i--);
    //                             throw Error::Recv();
    //                         }
    //                         client.state = READING_HEADERS;
    //                         client.bytes_received += new_bytes_received;
    //                         client.request.append(buffer, new_bytes_received);
    //                         size_t pos = client.request.find("\r\n\r\n", client.search_offset);
    //                         if (pos != std::string::npos)
    //                         {
    //                             client.state = READING_BODY;
    //                             std::cout << "Content starts here\n";
    //                             std::cout << client.request;
    //                         }
    //                         client.search_offset = ((client.search_offset + new_bytes_received) - 3);
    //                         // std::cout << buffer ;
    //                         // if (new_bytes_received > 0)
    //                         //     client.state = READING_HEADERS;
    //                         client.state = WRITING_RESPONSE;
    //                         std::string res = "HTTP/1.1 200 OK\r\nContent-Length: 11\r\n\r\nHello World";
    //                         ssize_t sent = send(fds[i].fd, res.c_str(), res.length(), 0);
    //                         if (sent < 0)
    //                         {
    //                             close(fds[i].fd);
    //                             fds.erase(fds.begin() + i--);
    //                             throw Error::Send();
    //                         }
    //                         Client.push_back(client);
    //                         std::cout << "----------- I'm here ----------\n";
    //                         // std::cout << buffer;
    //                     }
    //                 }
    //             }
    //             catch(const std::exception& e)
    //             {
    //                 std::cerr << e.what() << '\n';
    //             }
    //     }
    //     fds.insert(fds.end(), tmp_fds.begin(), tmp_fds.end());
    //     tmp_fds.clear();
    // }
    // if (Client[0].request.c_str())
    //     std::cout << Client[0].request;
    // for (size_t i = 0; i < fds.size(); i++)
    //     close(fds[i].fd);
    // close(fd_sock);






