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
#include <sys/types.h>
#include <sys/wait.h>
#include <limits>
#include <map>
#include "Error.hpp"
#include <sstream>
#include "../Request/ClientRequest.hpp"
// #include "include/request/RequestHelpers.hpp"
#define ERROR 1
#define SUCESS 0
#define MAX_KB 20000
#define MAX_MB 20
#define MAX_HEADER_SIZE 8192
#define MAX_RAM_BUFFER 8192
#define MAX_BY 22000000

// #include "include/request/ClientRequest.hpp"
// #include "include/request/RequestHelpers.hpp"


// --------------------------------------- Config File Header Part ------------------------------------- //

// class ClientRequest;



enum HttpStatus
{
        // 2xx Success
        HTTP_200_OK                     = 200,
        HTTP_201_CREATED                = 201,
        HTTP_204_NO_CONTENT             = 204,

        // 3xx Redirection
        HTTP_301_MOVED_PERMANENTLY      = 301,
        HTTP_302_FOUND                  = 302,
        HTTP_304_NOT_MODIFIED           = 304,

        // 4xx Client Errors
        HTTP_400_BAD_REQUEST            = 400,
        HTTP_403_FORBIDDEN              = 403,
        HTTP_404_NOT_FOUND              = 404,
        HTTP_405_METHOD_NOT_ALLOWED     = 405,
        HTTP_408_REQUEST_TIMEOUT        = 408,
        HTTP_409_CONFLICT               = 409,
        HTTP_410_GONE                   = 410,
        HTTP_411_LENGTH_REQUIRED        = 411,
        HTTP_413_PAYLOAD_TOO_LARGE      = 413,
        HTTP_414_URI_TOO_LONG           = 414,
        HTTP_415_UNSUPPORTED_MEDIA      = 415,

        // 5xx Server Errors
        HTTP_431_Request_Header_Fields_Too_Large = 431,
        HTTP_500_INTERNAL_SERVER_ERROR  = 500,
        HTTP_502_BAD_GATEWAY            = 502,
        HTTP_501_NOT_IMPLEMENTED        = 501,
        HTTP_504_GATEWAY_TIMEOUT        = 504,
        HTTP_505_HTTP_VERSION_NOT_SUPPORTED = 505
};

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


struct Client
{
    int fd;
    std::string request;
    // size_t content_length;
    // size_t bytes_received;
    std::string response;
    int port;
    int stream_file_fd; // call serveFile 
    off_t stream_bytes_remaining; // call serveFile 
    bool response_prepared;
    char    stream_buffer[4096];
    ssize_t stream_buffer_size;
    ssize_t stream_buffer_offset;
    bool cgi_started;
    // std::string body;
    // size_t end_of_header;
    ClientRequest parsed_request;
    
    Client()
    : fd(-1),
    port(0),
    stream_file_fd(-1),
    stream_bytes_remaining(0),
    response_prepared(false),
    stream_buffer_size(0),
    stream_buffer_offset(0),
    cgi_started(false)
    {}
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
        int get_listen_port();
        void setup(int port, const std::string& host);
        int  acceptClient();        
};

// ---------------------------- Multiplexing Class -------------------------------//

class Multiplexer 
{
    private:
        std::vector<Socket *>          _servers;
        std::map<int, Client>        _clients;
        std::vector<struct pollfd>     _pollfds;
        std::map<int , int>          _cgi_pipes;
        std::map<int, pid_t>        _cgi_pids;

        void _acceptNewClient(Socket *server);
        void _readClient(int fd);
        void _writeClient(int fd);
        void _removeClient(int fd);
        void    prepareResponse(Client &client); // Katwjd (prepare) response ghir mara wa7da. call despatcher just one call 
        bool    sendResponse(int fd, Client &client); // Sift l HTTP response (headers/body). 
        void    sendStreaming(int fd, Client &client); // Sift file kbira chunk b chunk
        void    disableWrite(int fd); // Salina, ma b9inach m7tajin POLLOUT. donc db server khaso isayn requst jdida.
        void    readCGI(int fd); // Read l CGI output, w sfto l client.
    public:
        char** env;
        Multiplexer();                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                           
        ~Multiplexer();
        void enableWrite(int fd);
        void addServer(Socket *s);
        int handleClient(int fd); // thi one executes CGI
        void run();
};

// -------------------------------- CGI Class -----------------------------------//

class CGI
{
    private:
        int stdin_pipe[2];
        int stdout_pipe[2];
        // int client_fd;
        pid_t pid;
        std::string request_path;
        std::string interpreter;
        std::string script;
        std::string body;
        bool extension_found;
        char** request_vars;
        std::vector<std::string> env_vars;
        int _find_interpreter(const Location_Config& conf);
        
    public:
        CGI(Client& client, const Location_Config& conf);
        ~CGI();
        void build_env_vars(Client& client);
        void writeToChild();
        // void readFromChild(int fd);
        int execute(std::map<int, pid_t>& map);
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
