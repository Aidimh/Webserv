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
#include <map>
#include "Error.hpp"
#define ERROR 1
#define SUCESS 0

class AFd
{
    protected:
        int fd;
    public:
        AFd();
        virtual ~AFd();
        int get_fd() const;

};

class Isocket : public AFd
{
    public:
};

typedef struct Location_Config
{
    std::string root;
    std::string path;
    std::string upload_path;
    std::vector<std::string> index_files;
    std::vector<std::string> allowed_methods;
    std::string cgi_extension;
    std::string cgi_path;
    std::map<int, std::string> error_pages;
    std::string _return;
    std::string autoindex;
    bool has_index;
    bool has_root;
    bool has_autoindex;
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
        std::string host;
        std::string server_name;
        std::string root;
        std::vector<std::string> index_files;
        size_t index_count;
        size_t max_body_size;
        bool body_size_is_GB;
        bool body_size_is_MB;
        bool body_size_is_KB;
        bool body_size_is_BT;
        std::map<int, std::string> error_pages;
        Location_Config location;

        // std::string location;
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

/*
listen
host
root
server_name
client_max_body_size
error_page

*/

class Conf_File
{
    public:
        static std::vector<Server_block> Servers;
        static std::vector<std::string> tokens;

        
};

enum ClientState
{
    READING_HEADERS,
    READING_BODY,
    PROCESSING,
    WRITING_RESPONSE,
    CLOSING,
};

struct Client
{
    int fd;
    size_t search_offset;
    std::string request;
    ClientState state;
    size_t content_length;
    size_t bytes_received;
    std::string response;
};

struct Request
{
    std::string method;
    std::string path;
    std::string version;
    std::string body;
    std::map<std::string, std::string> headers;
};




// ----------------------------- Signals Functions --------------------------------//
void handle_sigint(int sig);
void handle_sigquit(int sig);
void handle_sigstp(int sig);


// ----------------------------- Parsing Functions --------------------------------//
bool is_comment(std::string& line);
void parse_file();
void skip_white_spaces(std::string& line, size_t &i);
void skip_directive(std::string& line, size_t &i);
bool path_file_exists(std::string& name);
void validate_file();
void expected_token(std::vector<std::string>& vector, size_t &i, std::string& expected);
std::string next_token(std::vector<std::string>& vector , size_t &i);
void parse_config_file();
bool isKnownDirective(const std::string& token);
std::string next_token(std::vector<std::string>& tokens , size_t &i);
void expected_token(std::vector<std::string>& tokens, size_t &i, std::string& expected);
bool isKnownDirective(const std::string& token);
bool max_uploads_is_unit(size_t size, size_t index);
bool is_http_method(std::string& method);
bool is_autoindex_id(std::string& id);