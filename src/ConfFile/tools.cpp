#include "header.hpp"

// bool curly_brackets_even()

bool is_comment(std::string& line)
{
    size_t i = 0;
    while(line[i] == ' ' || line[i] == '\t')
        i++;
    if (line[i] == '#' || i == line.size())
        return true;
    else 
        return false;
    return true;
}

void skip_white_spaces(std::string& line, size_t &i)
{
    while (line[i] == ' ' || line[i] == '\t')
        i++;
}

void skip_directive(std::string& line, size_t &i)
{
    while (i < line.size() && !isspace(line[i]))
        i++;
}

bool path_file_exists(std::string& name)
{
    struct stat buffer;
    return (stat(name.c_str(), &buffer) == 0);
}

std::string next_token(std::vector<std::string>& tokens , size_t &i)
{
    if (i >= tokens.size())
        throw Error::UnexpectedEndOfFile();
    return tokens[++i];
}

void expected_token(std::vector<std::string>& tokens, size_t &i, std::string& expected)
{
    std::string token = next_token(tokens, i);
    if (token != expected)
        throw Error::SyntaxError();
}

bool isKnownDirective(const std::string& token)
{
    return (token == "listen" || token == "host" || token == "server_name" ||
            token == "root" || token == "index" || token == "client_max_body_size" ||
            token == "error_page" || token == "allowed_methods" || token == "autoindex" ||
            token == "return" || token == "cgi_extension" || token == "cgi_path" ||
            token == "upload_store");
}

bool max_uploads_is_unit(size_t size, size_t index)
{
    return (Conf_File::tokens[index + 1][size - 1] == 'M' 
            || Conf_File::tokens[index + 1][size - 1] == 'K' 
            || Conf_File::tokens[index + 1][size - 1] == 'G');
}

bool is_http_method(std::string& method)
{
    return (method == "GET" || method == "POST" || method == "DELETE");
}

bool is_cgi_extension(std::string& extension)
{
    return (extension == ".py" || extension == ".sh" || extension == ".php" || extension == ".bla");
}

bool is_autoindex_id(std::string& id)
{
    return (id == "on" || id == "off");
}

std::vector<std::string> split(const std::string& str, const std::string& delimiter)
{
    std::vector<std::string> tokens;
    std::size_t start = 0;
    std::size_t end = str.find(delimiter);

    while (end != std::string::npos) {
        tokens.push_back(str.substr(start, end - start));
        
        start = end + delimiter.length();
        
        end = str.find(delimiter, start);
    }
    
    tokens.push_back(str.substr(start));

    return tokens;
}