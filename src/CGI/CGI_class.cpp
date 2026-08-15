#include "header.hpp"
#include "../Logging/Logging.hpp"
static std::string get_header_value(const std::map<std::string, std::string>& headers, const std::string& key)
{
    std::map<std::string, std::string>::const_iterator it = headers.find(key);
    if (it == headers.end())
        return "";
    return it->second;
}

void CGI::build_env_vars(Client& client)
{
    env_vars.push_back("REQUEST_METHOD=" + client.parsed_request.getMethod());
    env_vars.push_back("PATH_INFO=" + client.parsed_request.getRequestPath());
    env_vars.push_back("SCRIPT_FILENAME=" + script);
    env_vars.push_back("CONTENT_TYPE=" + get_header_value(client.parsed_request.getHeaders(), "content_type"));
    env_vars.push_back("QUERY_STRING=");
    char buff[32];
    sprintf(buff, "%zu", client.parsed_request.getBodySize());
    std::string result = buff;
    env_vars.push_back("CONTENT_LENGTH=" + result);
    request_vars = new char *[env_vars.size() + 1];

    for (size_t i = 0; i < env_vars.size() ; i++)
        request_vars[i] = strdup(env_vars[i].c_str());
    request_vars[env_vars.size()] = NULL;
}


std::string CGI::get_interpreter() const
{
    return interpreter;
}

std::string CGI::get_script() const
{
    return script;
}

CGI::CGI(Client& client, const Location_Config& conf) : request_path(client.parsed_request.getRequestPath()) ,body(client.parsed_request.getBody())
{
    _find_interpreter(conf);
    build_env_vars(client);
}

CGI::~CGI()
{
    for(size_t i = 0; request_vars[i] != NULL; i++)
        free(request_vars[i]);
    delete[] request_vars;
}

void CGI::writeToChild()
{
    write(stdin_pipe[1], body.c_str(), body.size());
    close(stdin_pipe[1]);
}

// void CGI::readFromChild(int fd)
// {
//     char buffer[4096];

// }



int CGI::execute(std::map<int, pid_t>& map)
{
    char *argv[3];
	DEBUG("CGI") << "Preparing to execute CGI script: " << script << " with interpreter: " << interpreter;
    argv[0] = (char *)interpreter.c_str();
    argv[1] = (char *)script.c_str();
    argv[2] = NULL;
    if (pipe(stdin_pipe) == -1 || pipe(stdout_pipe) == -1)
        return ERROR;
    pid = fork();
    if (pid == -1)
        return ERROR;
    if (pid == 0)
    {
        if (dup2(stdin_pipe[0],STDIN_FILENO) == -1)
            perror("stdin\n");
        if (dup2(stdout_pipe[1],STDOUT_FILENO) == -1)
            perror("stdout\n");
		DEBUG("CGI") << "Executing CGI script: " << script << " with interpreter: " << interpreter;
        close(stdin_pipe[0]);
        close(stdin_pipe[1]);
        close(stdout_pipe[0]);
        close(stdout_pipe[1]);
		DEBUG("CGI") << "Closed stdin pipe" << stdin_pipe[0] << " and stdout pipe " 
		<< stdout_pipe[1] << " in child process";
        execve(interpreter.c_str(), argv, request_vars);
        exit(1);
    }
    else
    {
		DEBUG("CGI") << "Forked CGI process with pid: " << pid << " for script: " << script;
        close(stdin_pipe[0]); 
        close(stdout_pipe[1]);
		DEBUG("CGI") << "Closed stdin pipe" << stdin_pipe[0] << " and stdout pipe " 
		<< stdout_pipe[1] << " in parent process";
	}
    map[stdout_pipe[0]] = pid;
    return (stdout_pipe[0]);
}


void remove_char_at(std::string& str, size_t pos)
{
    if (pos < str.length())
    {
        str.erase(pos, 1);
    }
}

int CGI::_find_interpreter(const Location_Config& conf)
{
    size_t i = 0;
    size_t pos = request_path.rfind('.');
    if (pos == std::string::npos)
        return 1;
    // we remove the first / here cause execv can't relate to a file path that starts with /
    if (request_path.c_str() && request_path[0] == '/')
        remove_char_at(request_path, 0);
    // here we extract the extension .ext
    std::string extension = request_path.substr(pos - 1);
    while (i < conf.cgi_extensions.size())
    {
        if (conf.cgi_extensions[i] == extension)
        {
            this->interpreter = conf.cgi_paths[i];
            extension_found = true;
            break;
        }
        i++;
    }
    if (!extension_found)
        return ERROR;
    this->script = conf.root + request_path;
    return SUCESS;
}
