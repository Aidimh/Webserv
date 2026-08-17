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
#include <set>
#include <limits>
#include <map>
#include "../../includes/Errors/Error.hpp"
#include <sstream>
#include "../Request/ClientRequest.hpp"
// #include "include/request/RequestHelpers.hpp"
#define ERROR 1
#define SUCESS 0
#define MAX_KB 20000
#define MAX_MB 20
#define MAX_HEADER_SIZE 8192
#define MAX_RAM_BUFFER 8192
#define MAX_REQUEST_LINE_SIZE 2176
#define MAX_URI_SIZE 2048
#define MAX_BY 22000000
#define CGI_TIMEOUT 5
static const size_t CGI_CHUNK_SIZE = 65536;
// static const int EPOLL_TIMEOUT_MS = 1000;
static const size_t CGI_MAX_PENDING = 1048576;
// static const int EPOLL_TIMEOUT_MS = 1000;
static const int MAX_EVENTS = 256;
static const size_t MAX_CLIENTS = 4096;
static const time_t CLIENT_IDLE_TIMEOUT = 65;
static const int EPOLL_TIMEOUT_MS = 1000;
// static const int MAX_EVENTS = 256;



// #include "include/request/ClientRequest.hpp"
// #include "include/request/RequestHelpers.hpp"


// --------------------------------------- Config File Header Part ------------------------------------- //

// class ClientRequest;
// int client_uniq_id;


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
    bool has_max_body_size;
    long max_body_size;
    Location_Config()
    {
        has_index = false;
        has_root = false;
        has_autoindex = false;
        has_max_body_size = false;

        cgi_paths_index = 0;
        cgi_extns_index = 0;
        max_body_size = 0;
    };

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
        bool server_has_autoindex;
        size_t ports_count;
        std::vector<int> listen_port;
        std::vector<std::string> listen_port_str;
        std::string host;
        std::string server_name;
        std::string root;
        std::string server_auto_index;
        std::vector<std::string> index_files;
        size_t index_count;
        long max_body_size;
        std::map<int, std::string> error_pages;
        std::vector<Location_Config> location;
        size_t location_count;
        std::vector<std::string> methods;
        std::string default_file;
        std::string autoindex;
        Server_block()
        {
            server_found = false;
            host_found = false;
            location_found = false;
            root_found = false;
            server_name_found = false;
            listen_found = false;
            index_found = false;
            server_has_autoindex = false;
            error_page_found = false;
            client_max_body_found = false;
            ports_count = 0;
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

struct CgiState
{
    pid_t       pid;
    int         stdin_fd;       // pipe the request body is written into
    int         stdout_fd;      // pipe the CGI answer is read from
    int         body_fd;        // temp file holding the request body, -1 when in RAM
    std::string body_buffer;    // request body bytes not forwarded yet
    std::string header_buffer;  // CGI header block being collected
    bool        headers_done;   // HTTP header block already handed to the client
    bool        running;
    time_t      last_activity;

    CgiState()
    : pid(-1),
      stdin_fd(-1),
      stdout_fd(-1),
      body_fd(-1),
      headers_done(false),
      running(false),
      last_activity(0)
    {}

    void clear()
    {
        pid = -1;
        stdin_fd = -1;
        stdout_fd = -1;
        body_fd = -1;
        body_buffer.clear();
        header_buffer.clear();
        headers_done = false;
        running = false;
        last_activity = 0;
    }
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
    // int client_id;
    std::string session_id;
    // std::string body;
    // size_t end_of_header;
    bool close_after_response;
    time_t last_activity;
    // bool last_activity;
    ClientRequest parsed_request;
    CgiState cgi;
    
	void reset()
    {
        cgi.clear();
        request.clear();
        response.clear();
        response_prepared = false;
        cgi_started = false;
        if (stream_file_fd != -1)
        {
            close(stream_file_fd);
            stream_file_fd = -1;
        }
        stream_bytes_remaining = 0;
        stream_buffer_size = 0;
        stream_buffer_offset = 0;
        parsed_request.reset();
    }

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
        std::vector<Socket *>           _servers;
        std::map<int, Client>           _clients;
        // std::vector<struct pollfd>      _pollfds;
        std::map<int , int>             _cgi_pipes;
        std::map<int, pid_t>            _cgi_pids;
        std::map<int, uint32_t>         _watched;    /* mirror of the kernel interest list */
        std::set<int>                   _dead_fds;
        std::map<int, time_t>           cgi_timeouts;
        std::map<int, std::string>      client_ids;
        int                             _epoll_fd;
    public:
        void                            registerCgiPipes(Client& client);
        bool                            startCgi(Client& client , const Server_block& server);
        void                            _acceptNewClient(Socket *server);
        void                            releaseCgi(Client& client);
        void                            _readClient(int fd);
        void                            _writeClient(int fd);
        void                            _removeClient(int fd);
		void							handlePeerShutdown(int fd, Client& client);
        // std::string&                    _fill_cgi_response(int fd);
        void                            prepareResponse(Client &client); // Katwjd (prepare) response ghir mara wa7da. call despatcher just one call 
        bool                            sendResponse(int fd, Client &client); // Sift l HTTP response (headers/body). 
        void                            sendStreaming(int fd, Client &client); // Sift file kbira chunk b chunk
        // void                            disableWrite(int fd); // Salina, ma b9inach m7tajin POLLOUT. donc db server khaso isayn request jdida.
        bool                            is_cgi(const std::string& path); // this func checks weather a path is cgi_path
        bool                            is_in_cgi_list(std::string& ext);
        bool                            openCgiBodySource(Client& client);
        void                            writeCgiInput(int pipe_fd);
        void                            closeCgiInput(Client& client);
        void                            killTimedOutCgi();
        void                            readCgiOutput(int pipe_fd);
        void                            appendCgiPayload(Client& client, const char* data, size_t size);
        void                            emitCgiHeaders(Client& client);
        void                            finishCgiOutput(Client& client);
        void                            applyCgiBackPressure();
        void                            dispatchEvents(struct epoll_event* events, int count);
        void                            handleEvent(int fd, uint32_t revents);
        bool                            handleServerEvent(int fd, uint32_t revents);
        bool                            handleCgiEvent(int fd, uint32_t revents);
        void                            handleClientEvent(int fd, uint32_t revents);
        Client*                         findClient(int fd);
        Client*                         findClientByPipe(int pipe_fd);
        void                            advanceRequest(int fd, Client& client);
        void                            finishResponse(int fd, Client& client);
        bool                            isEvictable(const Client& client) const;
        void                            evictOldestClient();
        void                            closeIdleClients();
        void                            addFd(int fd, uint32_t events);
        void                            removeFd(int fd);
        void                            setEvents(int fd, uint32_t events);
        bool                            isRegistered(int fd) const;
        void                            disableWrite(int fd);

        // void                            readCGI(int fd); // Read l CGI output, w sfto l client.
        // std::string                     _generateClientID(int fd);
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
        CGI(Client& client, const Location_Config& conf, const Server_block& server);
        ~CGI();
        void addEnv(const std::string& key, const std::string& value);
        void buildEnvArray();
        void addRequestHeaders(const Client& client);
        // void build_env_vars(Client& client); old one
        void build_env_vars(Client& client, const Server_block& server);
        std::string get_interpreter() const;
        std::string get_script() const;
        int get_input_fd();
        int get_output_fd();
        int get_pid();

        void writeToChild();
        bool _find_interpreter(const Location_Config& conf, const Server_block& server);
        // void readFromChild(int fd);
        bool execute();
        bool openPipes();
        void runChild();
        bool isRunnable() const;
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
int every_server_has_listen_port();
std::string next_token(std::vector<std::string>& tokens , size_t &i);
void expected_token(std::vector<std::string>& tokens, size_t &i, std::string& expected);
bool isKnownDirective(const std::string& token);
bool is_http_method(std::string& method);
bool is_autoindex_id(std::string& id);
void parse_root_path(size_t &index);
void parse_autoindex(size_t &index);
void parse_upload_store(size_t &index);
void parse_methods(size_t &index);
void parse_cgi_extension(size_t &index);
void parse_cgi_path(size_t &index);
void parse_return(size_t &index);


// ----------------------------- Init Functions --------------------------------//

void initServerBlock(Server_block& server);
void initLocationConfig(Location_Config& loc);
