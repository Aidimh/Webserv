#pragma once
#include <iostream>
#include <sys/socket.h>
// #include <netinit/in.h>
#include <arpa/inet.h>
#include <vector>
#include <sys/select.h>
#include <poll.h>
#include <sys/epoll.h>
#include <cstring>
#include <stdio.h>
#include <unistd.h>
#include <iostream>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <cstring>
#include <fcntl.h>
#include <csignal>
#include <string>
#include <cerrno>
#include <fstream>
#include <cstdlib>
#include <sys/stat.h>
#include <sys/wait.h>
#include <map>
#include "Error.hpp"
#define ERROR 1
#define SUCESS 0
#define MAX_KB 20000
#define MAX_MB 20
#define MAX_BY 22000000

#include "include/request/ClientRequest.hpp"

typedef struct Location_Config
{
    std::string root;
    std::string path;
    std::string upload_path;
    std::vector<std::string> index_files;
    std::vector<std::string> allowed_methods;
    std::vector<std::string> cgi_extensions;
    std::vector<std::string> cgi_paths;
    std::map<int, std::string> error_pages;
    std::string _return;
    std::string autoindex;
    bool has_index;
    bool has_root;
    bool has_autoindex;
    size_t cgi_paths_index;
    size_t cgi_extns_index;
} Location_Config;


class Server_block
{
    public:
        bool server_found;
        bool host_found;
        bool location_found;
        bool root_found;
        bool server_name_found;
        bool listen_found;
        bool index_found;
        bool error_page_found;
        bool client_max_body_found;
        bool uploadLimits;
        int listen_port;
        std::string listen_port_str;
        std::string host;
        std::string server_name;
        std::string root;
        std::vector<std::string> index_files;
        size_t index_count;
        long max_body_size;
        bool body_size_is_MB;
        bool body_size_is_KB;
        bool body_size_is_BT;
        std::map<int, std::string> error_pages;
        std::vector<Location_Config> location;
        size_t location_count;
        std::vector<std::string> methods;
        std::string default_file;
        std::string autoindex;
        void reset_flags()
        {
            server_found = false;
            host_found = false;
            location_found = false;
            root_found = false;
            server_name_found = false;
            listen_found = false;
            index_found = false;
            error_page_found = false;
            client_max_body_found = false;
        }
};
