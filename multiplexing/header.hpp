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
#define MAX_KB 20000
#define MAX_MB 20
#define MAX_BY 22000000


// --------------------------------------- Config File Header Part ------------------------------------- //

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



struct Request
{
    std::string method;
    std::string method_path;
    std::string cgi_extension;
    std::string version;
    std::string body;
    std::string cgi;
    std::map<std::string, std::string> headers;
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
    std::string body;
    size_t end_of_header;
    struct Request parsed_request;
};

class AFd
{
    protected:
        int fd;
    public:
        AFd();
        virtual ~AFd();
        int get_fd() const;
};


class Socket : public AFd
{
    private:
        int         _port;
        std::string _host;

    public:
        Socket();
        ~Socket();
        std::string GetClientIp();
        void setup(int port, const std::string& host);
        int  acceptClient();        
};

// ---------------------------- Multiplexing Class -------------------------------//


class Multiplexer {
private:
    std::vector<Socket *>          _servers;
    std::map<int, Client>        _clients;
    std::vector<struct pollfd>     _pollfds;

    void _acceptNewClient(Socket *server);
    void _readClient(int fd);
    void _writeClient(int fd);
    void _removeClient(int fd);

public:
    Multiplexer();                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                           
    ~Multiplexer();
    void enableWrite(int fd);
    void addServer(Socket *s);
    int handleClient(int fd);
    void run();
};


// -------------------------------- CGI Class -----------------------------------//


class CGI
{
    private:
        int stdin_pipe[2];
        int stdout_pipe[2];
        int client_fd;
        pid_t pid;
        std::string path;
        std::string script;
        std::string body;
        std::vector<std::string> env_vars;
        
    public:
        CGI(const Client& client, const Location_Config& conf);
        void writeToChild(int fd);
        void readFromChild(int fd);
        int execute();
};



// ----------------------------- Signals Functions --------------------------------//
void handle_sigint(int sig);
void handle_sigquit(int sig);
void handle_sigstp(int sig);


// ----------------------------- Parsing Functions --------------------------------//
bool is_comment(std::string& line);
bool is_cgi_extension(std::string& extension);
std::vector<std::string> split(const std::string& str, const std::string& delimiter);
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
void parse_root_path(size_t &index);
void parse_autoindex(size_t &index);
void parse_upload_store(size_t &index);
void parse_methods(size_t &index);
void parse_cgi_extension(size_t &index);
void parse_cgi_path(size_t &index);
void parse_return(size_t &index);