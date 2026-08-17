#include "../../includes/multiplexing/header.hpp"
#include "../Logging/Logging.hpp"




static std::string get_header_value(const std::map<std::string, std::string>& headers, const std::string& key)
{
    std::map<std::string, std::string>::const_iterator it = headers.find(key);
    if (it == headers.end())
        return "";
    return it->second;
}

/////////////////////////////////////////// 7: cgi_env_vars //////////////////////////////////////////////////////////////////


static std::string toEnvName(const std::string& header)
{
    std::string name = "HTTP_";

    for (size_t i = 0; i < header.size(); i++)
    {
        if (header[i] == '-')
            name += '_';
        else
            name += static_cast<char>(toupper(static_cast<unsigned char>(header[i])));
    }
    return name;
}

static std::string sizeToString(size_t value)
{
    std::ostringstream out;

    out << value;
    return out.str();
}

void CGI::build_env_vars(Client& client, const Server_block& server)
{
    const ClientRequest& request = client.parsed_request;

    addEnv("GATEWAY_INTERFACE", "CGI/1.1");
    addEnv("SERVER_SOFTWARE", "webserv/1.0");
    addEnv("SERVER_PROTOCOL", "HTTP/1.1");
    addEnv("SERVER_NAME", server.server_name);
    addEnv("SERVER_PORT", sizeToString(static_cast<size_t>(client.port)));
    addEnv("REQUEST_METHOD", request.getMethod());
    addEnv("REQUEST_URI", request.getRequestPath());
    addEnv("PATH_INFO", request.getRequestPath());
    addEnv("PATH_TRANSLATED", script);
    addEnv("SCRIPT_NAME", request.getRequestPath());
    addEnv("SCRIPT_FILENAME", script);
    addEnv("QUERY_STRING", request.getQueryString());
    addEnv("CONTENT_TYPE", get_header_value(request.getHeaders(), "content-type"));
    addEnv("CONTENT_LENGTH", sizeToString(request.getBodySize()));
    addEnv("REDIRECT_STATUS", "200");
    addRequestHeaders(client);
    buildEnvArray();
}

// void CGI::build_env_vars(Client& client)
// {
//     env_vars.push_back("REQUEST_METHOD=" + client.parsed_request.getMethod());
//     env_vars.push_back("PATH_INFO=" + client.parsed_request.getRequestPath());
//     env_vars.push_back("SCRIPT_FILENAME=" + script);
//     env_vars.push_back("CONTENT_TYPE=" + get_header_value(client.parsed_request.getHeaders(), "content-type"));
//     env_vars.push_back("QUERY_STRING=");
//     char buff[32];
//     sprintf(buff, "%zu", client.parsed_request.getBodySize());
//     std::string result = buff;
//     env_vars.push_back("CONTENT_LENGTH=" + result);
//     request_vars = new char *[env_vars.size() + 1];

//     for (size_t i = 0; i < env_vars.size() ; i++)
//         request_vars[i] = strdup(env_vars[i].c_str());
//     request_vars[env_vars.size()] = NULL;
// }

void CGI::addRequestHeaders(const Client& client)
{
    const std::map<std::string, std::string>& headers = client.parsed_request.getHeaders();
    std::map<std::string, std::string>::const_iterator it;

    for (it = headers.begin(); it != headers.end(); ++it)
        addEnv(toEnvName(it->first), it->second);
}

void CGI::buildEnvArray()
{
    request_vars = new char *[env_vars.size() + 1];

    for (size_t i = 0; i < env_vars.size(); i++)
        request_vars[i] = strdup(env_vars[i].c_str());
    request_vars[env_vars.size()] = NULL;
}



void CGI::addEnv(const std::string& key, const std::string& value)
{
    env_vars.push_back(key + "=" + value);
}

std::string CGI::get_interpreter() const
{
    return interpreter;
}

//////////////////////////////////////////////////////////////////////////////////////////

///////////////////////////////////// 9 : cgi_script_path ////////////////////////////////

std::string joinPath(const std::string& root, const std::string& path)
{
    std::string base = root;

    if (!base.empty() && base[base.size() - 1] == '/')
        base.erase(base.size() - 1);
    if (path.empty() || path[0] != '/')
        return base + "/" + path;
    return base + path;
}

bool CGI::_find_interpreter(const Location_Config& conf, const Server_block& server)
{
    size_t pos = request_path.rfind('.');

    if (pos == std::string::npos)
        return false;

    std::string extension = request_path.substr(pos);

    for (size_t i = 0; i < conf.cgi_extensions.size(); i++)
    {
        if (conf.cgi_extensions[i] != extension)
            continue;
        interpreter = conf.cgi_paths[i];
        extension_found = true;
        break;
    }
    if (!extension_found)
    {
        DEBUG("CGI") << "_find_interpreter: no interpreter configured for extension=" << extension;
        return false;
    }

    std::string root = server.root;
    if (conf.has_root && !conf.root.empty())
        root = conf.root;
    script = joinPath(root, request_path);
    DEBUG("CGI") << "_find_interpreter: extension=" << extension
                 << " interpreter=" << interpreter << " script=" << script;
    return true;
}



CGI::CGI(Client& client, const Location_Config& conf, const Server_block& server)
: pid(-1),
  request_path(client.parsed_request.getRequestPath()),
  extension_found(false),
  request_vars(NULL)
{
    stdin_pipe[0] = -1;
    stdin_pipe[1] = -1;
    stdout_pipe[0] = -1;
    stdout_pipe[1] = -1;
    if (!_find_interpreter(conf, server))
        return;
    build_env_vars(client, server);
}


bool CGI::isRunnable() const
{
    return extension_found;
}

// CGI::~CGI()
// {
//     for(size_t i = 0; request_vars[i] != NULL; i++)
//         free(request_vars[i]);
//     delete[] request_vars;
// }

CGI::~CGI()
{
    if (request_vars == NULL)
        return;
    for (size_t i = 0; request_vars[i] != NULL; i++)
        free(request_vars[i]);
    delete[] request_vars;
}

//////////////////////////////////////////////////////////////////////////////////////////


std::string CGI::get_script() const
{
    return script;
}

// CGI::CGI(Client& client, const Location_Config& conf) : request_path(client.parsed_request.getRequestPath()) ,body(client.parsed_request.getBody())
// {
//     _find_interpreter(conf);
//     build_env_vars(client);
// }


void CGI::writeToChild()
{
    write(stdin_pipe[1], body.c_str(), body.size());
    DEBUG("CGI") << "writeToChild: wrote " << body.size() << " bytes to stdin pipe fd=" << stdin_pipe[1];
    close(stdin_pipe[1]);
    DEBUG("CGI") << "writeToChild: closed stdin pipe fd=" << stdin_pipe[1];
}

// void CGI::readFromChild(int fd)
// {
//     char buffer[4096];

// }


/////////////////////////////////// 8 : cgi_body_lost & deadlock //////////////////////////////////////////////////

void Multiplexer::writeCgiInput(int pipe_fd)
{
    Client* client = findClientByPipe(pipe_fd);

    if (client == NULL)
        return;
    if (client->cgi.body_buffer.empty() && !refillCgiBody(client->cgi))
    {
        closeCgiInput(*client);
        return;
    }

    ssize_t written = write(pipe_fd, client->cgi.body_buffer.data(), client->cgi.body_buffer.size());
    if (written <= 0)
    {
        DEBUG("Multiplexer") << "writeCgiInput: cgi stopped reading fd=" << pipe_fd;
        closeCgiInput(*client);
        return;
    }
    client->cgi.body_buffer.erase(0, written);
    client->cgi.last_activity = time(NULL);
    DDEBUG("Multiplexer") << "writeCgiInput: wrote " << written << " body bytes to cgi fd=" << pipe_fd;
}


void Multiplexer::closeCgiInput(Client& client)
{
    if (client.cgi.stdin_fd != -1)
    {
        removeFd(client.cgi.stdin_fd);
        _cgi_pipes.erase(client.cgi.stdin_fd);
        close(client.cgi.stdin_fd);
        DEBUG("Multiplexer") << "closeCgiInput: closed cgi input fd=" << client.cgi.stdin_fd
                             << " client fd=" << client.fd;
        client.cgi.stdin_fd = -1;
    }
    if (client.cgi.body_fd != -1)
    {
        close(client.cgi.body_fd);
        client.cgi.body_fd = -1;
    }
    client.cgi.body_buffer.clear();
}

static bool refillCgiBody(CgiState& cgi)
{
    char buffer[CGI_CHUNK_SIZE];

    if (cgi.body_fd == -1)
        return false;

    ssize_t count = read(cgi.body_fd, buffer, sizeof(buffer));
    if (count <= 0)
        return false;
    cgi.body_buffer.append(buffer, count);
    return true;
}

void CGI::runChild()
{
    char *argv[3];

    argv[0] = const_cast<char *>(interpreter.c_str());
    argv[1] = const_cast<char *>(script.c_str());
    argv[2] = NULL;

    if (dup2(stdin_pipe[0], STDIN_FILENO) == -1 || dup2(stdout_pipe[1], STDOUT_FILENO) == -1)
        _exit(1);

    close(stdin_pipe[0]);
    close(stdin_pipe[1]);
    close(stdout_pipe[0]);
    close(stdout_pipe[1]);

    execve(interpreter.c_str(), argv, request_vars);
    _exit(1);
}

bool Multiplexer::openCgiBodySource(Client& client)
{
    const ClientRequest& request = client.parsed_request;

    if (!request.usesTmpFile())
    {
        client.cgi.body_buffer = request.getBody();
        return true;
    }
    client.cgi.body_fd = open(request.getTmpFilePath().c_str(), O_RDONLY);
    if (client.cgi.body_fd == -1)
    {
        ERR() << "Multiplexer::openCgiBodySource: open body file failed path="
              << request.getTmpFilePath() << ": " << strerror(errno);
        return false;
    }
    unlink(request.getTmpFilePath().c_str());
    return true;
}



bool CGI::openPipes()
{
    if (pipe(stdin_pipe) == -1)
    {
        ERR() << "CGI::openPipes: pipe failed: " << strerror(errno);
        return false;
    }
    if (pipe(stdout_pipe) == -1)
    {
        ERR() << "CGI::openPipes: pipe failed: " << strerror(errno);
        close(stdin_pipe[0]);
        close(stdin_pipe[1]);
        stdin_pipe[0] = -1;
        stdin_pipe[1] = -1;
        return false;
    }
    DEBUG("CGI") << "openPipes: stdin pipe fd=" << stdin_pipe[0] << "," << stdin_pipe[1]
                 << " stdout pipe fd=" << stdout_pipe[0] << "," << stdout_pipe[1];
    return true;
}

bool CGI::execute()
{
    if (!extension_found || !openPipes())
        return false;

    pid = fork();
    if (pid == -1)
    {
        ERR() << "CGI::execute: fork failed: " << strerror(errno);
        close(stdin_pipe[0]);
        close(stdin_pipe[1]);
        close(stdout_pipe[0]);
        close(stdout_pipe[1]);
        return false;
    }
    if (pid == 0)
        runChild();

    close(stdin_pipe[0]);
    close(stdout_pipe[1]);
    stdin_pipe[0] = -1;
    stdout_pipe[1] = -1;
    fcntl(stdin_pipe[1], F_SETFL, O_NONBLOCK);
    fcntl(stdout_pipe[0], F_SETFL, O_NONBLOCK);
    DEBUG("CGI") << "execute: forked pid=" << pid << " script=" << script
                 << " input fd=" << stdin_pipe[1] << " output fd=" << stdout_pipe[0];
    return true;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////



////////////////////////////////// 14: cgi output is not http ///////////////////////////////////////////////////////////////////////


static std::vector<std::string> splitHeaderLines(const std::string& block)
{
    std::vector<std::string> lines;
    size_t start = 0;

    while (start < block.size())
    {
        size_t end = block.find('\n', start);
        std::string line = (end == std::string::npos)
                         ? block.substr(start)
                         : block.substr(start, end - start);

        if (!line.empty() && line[line.size() - 1] == '\r')
            line.erase(line.size() - 1);
        if (!line.empty())
            lines.push_back(line);
        if (end == std::string::npos)
            break;
        start = end + 1;
    }
    return lines;
}


static bool isStatusHeader(const std::string& line)
{
    return line.size() >= 7 && strncasecmp(line.c_str(), "Status:", 7) == 0;
}

static std::string cgiStatusLine(const std::vector<std::string>& lines)
{
    for (size_t i = 0; i < lines.size(); i++)
    {
        if (!isStatusHeader(lines[i]))
            continue;

        std::string value = lines[i].substr(7);
        size_t begin = value.find_first_not_of(" \t");

        if (begin != std::string::npos)
            return value.substr(begin);
    }
    return "200 OK";
}

static std::string cgiForwardedHeaders(const std::vector<std::string>& lines)
{
    std::string headers;

    for (size_t i = 0; i < lines.size(); i++)
    {
        if (isStatusHeader(lines[i]))
            continue;
        headers += lines[i] + "\r\n";
    }
    return headers;
}

static std::string buildCgiResponseHead(const std::string& cgiHeaderBlock)
{
    std::vector<std::string> lines = splitHeaderLines(cgiHeaderBlock);
    std::string head;

    head += "HTTP/1.1 " + cgiStatusLine(lines) + "\r\n";
    head += cgiForwardedHeaders(lines);
    head += "Transfer-Encoding: chunked\r\n";
    head += "Connection: close\r\n";
    head += "\r\n";
    return head;
}

static void appendChunk(std::string& out, const char* data, size_t size)
{
    std::ostringstream header;

    if (size == 0)
        return;
    header << std::hex << size << "\r\n";
    out += header.str();
    out.append(data, size);
    out += "\r\n";
}

static size_t findHeaderBlockEnd(const std::string& buffer, size_t& terminatorSize)
{
    size_t position = buffer.find("\r\n\r\n");

    if (position != std::string::npos)
    {
        terminatorSize = 4;
        return position;
    }
    position = buffer.find("\n\n");
    if (position != std::string::npos)
    {
        terminatorSize = 2;
        return position;
    }
    return std::string::npos;
}

void Multiplexer::readCgiOutput(int pipe_fd)
{
    Client* client = findClientByPipe(pipe_fd);
    char buffer[CGI_CHUNK_SIZE];

    if (client == NULL)
        return;

    ssize_t count = read(pipe_fd, buffer, sizeof(buffer));
    if (count <= 0)
    {
        finishCgiOutput(*client);
        return;
    }
    client->cgi.last_activity = time(NULL);
    appendCgiPayload(*client, buffer, count);
    enableWrite(client->fd);
    DDEBUG("Multiplexer") << "readCgiOutput: read " << count << " bytes from cgi fd=" << pipe_fd;
}

void Multiplexer::appendCgiPayload(Client& client, const char* data, size_t size)
{
    if (!client.cgi.headers_done)
    {
        client.cgi.header_buffer.append(data, size);
        emitCgiHeaders(client);
        return;
    }
    appendChunk(client.response, data, size);
}

void Multiplexer::emitCgiHeaders(Client& client)
{
    size_t terminatorSize = 0;
    size_t end = findHeaderBlockEnd(client.cgi.header_buffer, terminatorSize);

    if (end == std::string::npos)
    {
        if (client.cgi.header_buffer.size() < MAX_HEADER_SIZE)
            return;
        end = 0;
        terminatorSize = 0;
    }

    std::string headerBlock = client.cgi.header_buffer.substr(0, end);
    std::string payload = client.cgi.header_buffer.substr(end + terminatorSize);

    client.response += buildCgiResponseHead(headerBlock);
    client.cgi.headers_done = true;
    client.cgi.header_buffer.clear();
    appendChunk(client.response, payload.data(), payload.size());
}

void Multiplexer::finishCgiOutput(Client& client)
{
    if (!client.cgi.headers_done)
    {
        std::string payload = client.cgi.header_buffer;

        client.cgi.header_buffer.clear();
        client.response += buildCgiResponseHead("");
        client.cgi.headers_done = true;
        appendChunk(client.response, payload.data(), payload.size());
    }
    client.response += "0\r\n\r\n";
    DEBUG("Multiplexer") << "finishCgiOutput: cgi answer complete client fd=" << client.fd;
    releaseCgi(client);
    enableWrite(client.fd);
}

void Multiplexer::applyCgiBackPressure()
{
    std::map<int, Client>::iterator it;

    for (it = _clients.begin(); it != _clients.end(); ++it)
    {
        Client& client = it->second;

        if (!client.cgi.running || client.cgi.stdout_fd == -1)
            continue;
        if (client.response.size() >= CGI_MAX_PENDING)
            setEvents(client.cgi.stdout_fd, 0);
        else
            setEvents(client.cgi.stdout_fd, EPOLLIN);
    }
}

// void remove_char_at(std::string& str, size_t pos)
// {
//     if (pos < str.length())
//     {
//         str.erase(pos, 1);
//     }
// }

// int CGI::_find_interpreter(const Location_Config& conf)
// {
//     size_t i = 0;
//     size_t pos = request_path.rfind('.');
//     if (pos == std::string::npos)
//         return 1;
//     // we remove the first / here cause execv can't relate to a file path that starts with /
//     if (request_path.c_str() && request_path[0] == '/')
//         remove_char_at(request_path, 0);
//     // here we extract the extension .ext
//     std::string extension = request_path.substr(pos - 1);
//     while (i < conf.cgi_extensions.size())
//     {
//         if (conf.cgi_extensions[i] == extension)
//         {
//             this->interpreter = conf.cgi_paths[i];
//             extension_found = true;
//             break;
//         }
//         i++;
//     }
//     if (!extension_found)
//     {
//         DEBUG("CGI") << "_find_interpreter: no interpreter configured for extension=" << extension;
//         return ERROR;
//     }
//     this->script = conf.root + request_path;
//     DEBUG("CGI") << "_find_interpreter: extension=" << extension
//                  << " interpreter=" << interpreter << " script=" << script;
//     return SUCESS;
// }
