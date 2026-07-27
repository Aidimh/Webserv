#ifndef Multiplexing_HEADER_HPP
#define Multiplexing_HEADER_HPP

#include "Core/ConfigTypes.hpp"
#include "Errors/Error.hpp"
#include "Request/ClientRequest.hpp"
#include "Multiplexing/Client.hpp"

#include <map>
#include <poll.h>
#include <string>
#include <sys/types.h>
#include <vector>

#define MAX_HEADER_SIZE 8192
#define MAX_RAM_BUFFER 8192

enum HttpStatus
{
    HTTP_200_OK = 200,
    HTTP_201_CREATED = 201,
    HTTP_204_NO_CONTENT = 204,
    HTTP_301_MOVED_PERMANENTLY = 301,
    HTTP_302_FOUND = 302,
    HTTP_304_NOT_MODIFIED = 304,
    HTTP_400_BAD_REQUEST = 400,
    HTTP_403_FORBIDDEN = 403,
    HTTP_404_NOT_FOUND = 404,
    HTTP_405_METHOD_NOT_ALLOWED = 405,
    HTTP_408_REQUEST_TIMEOUT = 408,
    HTTP_409_CONFLICT = 409,
    HTTP_410_GONE = 410,
    HTTP_411_LENGTH_REQUIRED = 411,
    HTTP_413_PAYLOAD_TOO_LARGE = 413,
    HTTP_414_URI_TOO_LONG = 414,
    HTTP_415_UNSUPPORTED_MEDIA = 415,
    HTTP_500_INTERNAL_SERVER_ERROR = 500,
    HTTP_502_BAD_GATEWAY = 502,
    HTTP_504_GATEWAY_TIMEOUT = 504,
    HTTP_505_HTTP_VERSION_NOT_SUPPORTED = 505
};

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
    std::string response;
    int port;
    ClientRequest parsed_request;
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
        int _port;
        std::string _host;

    public:
        Socket();
        ~Socket();
        std::string GetClientIp();
        int get_listen_port();
        void setup(int port, const std::string& host);
        int acceptClient();
};

class Multiplexer
{
    private:
        std::vector<Socket *> _servers;
        std::map<int, Client> _clients;
        std::vector<struct pollfd> _pollfds;
        std::map<int, int> _cgi_pipes;
        std::map<int, pid_t> _cgi_pids;

        void _acceptNewClient(Socket *server);
        void _readClient(int fd);
        void _writeClient(int fd);
        void _removeClient(int fd);

    public:
        char** env;
        Multiplexer();
        ~Multiplexer();
        void enableWrite(int fd);
        void addServer(Socket *s);
        int handleClient(int fd);
        void run();
};

class CGI
{
    private:
        int stdin_pipe[2];
        int stdout_pipe[2];
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
        int execute(std::map<int, pid_t>& map);
};

void handle_sigint(int sig);
void handle_sigquit(int sig);
void handle_sigstp(int sig);

bool is_comment(std::string& line);
bool is_cgi_extension(std::string& extension);
std::vector<std::string> split(const std::string& str, const std::string& delimiter);
void parse_file();
void skip_white_spaces(std::string& line, size_t& i);
void skip_directive(std::string& line, size_t& i);
bool path_file_exists(std::string& name);
void validate_file();
void expected_token(std::vector<std::string>& vector, size_t& i, std::string& expected);
std::string next_token(std::vector<std::string>& vector, size_t& i);
void parse_config_file();
bool isKnownDirective(const std::string& token);
bool max_uploads_is_unit(size_t size, size_t index);
bool is_http_method(std::string& method);
bool is_autoindex_id(std::string& id);
void parse_root_path(size_t& index);
void parse_autoindex(size_t& index);
void parse_upload_store(size_t& index);
void parse_methods(size_t& index);
void parse_cgi_extension(size_t& index);
void parse_cgi_path(size_t& index);
void parse_return(size_t& index);

#endif
