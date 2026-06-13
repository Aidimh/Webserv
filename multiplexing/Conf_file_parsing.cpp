#include "header.hpp"
#include "Error.hpp"

extern int server_index;



// void parse_cgi_path(size_t &index)
// {
//     // size_t i = 0;
//     if (index + 2 >= Conf_File::tokens.size())
//         throw Error::Root();
//     if (Conf_File::tokens[index + 2] != ";")
//         throw Error::Root();
// }

void parse_return(size_t &index)
{
    // size_t i = 0;
    if (index + 2 >= Conf_File::tokens.size())
        throw Error::Root();
    if (Conf_File::tokens[index + 2] != ";")
        throw Error::Root();
    
}

void parse_root(size_t &index)
{
    // size_t i = 0;
    if (index + 2 >= Conf_File::tokens.size())
        throw Error::Root();
    if (Conf_File::tokens[index + 2] != ";")
        throw Error::Root();
    Conf_File::Servers[server_index].root = next_token(Conf_File::tokens, index);
    if (!path_file_exists(Conf_File::Servers[server_index].root))
        throw Error::Root();
    index += 2;
}

void parse_host(size_t &index)
{
    size_t i = 0;
    size_t dot_count = 0;

    if (index + 1 >= Conf_File::tokens.size() || index + 2 >= Conf_File::tokens.size())
        throw Error::Host_ip();
    if (Conf_File::tokens[index + 2] != ";")
        throw Error::Host_ip();
    if (Conf_File::tokens[index + 1].size() < 7)
        throw Error::Host_ip();
    for (int octet = 0; octet < 4; octet++)
    {
        if (i == Conf_File::tokens[index + 1].size() || !isdigit(Conf_File::tokens[index + 1][i]))
            throw Error::Host_ip();

        if (Conf_File::tokens[index + 1][i] == '0' && i + 1 < Conf_File::tokens[index + 1].size() && isdigit(Conf_File::tokens[index + 1][i + 1]))
            throw Error::Host_ip();
        char octet_buf[4];
        size_t j = 0;
        while (i < Conf_File::tokens[index + 1].size() && isdigit(Conf_File::tokens[index + 1][i]))
        {
            if (j == 3)
                throw Error::Host_ip();
            octet_buf[j++] = Conf_File::tokens[index + 1][i++];
        }
        octet_buf[j] = '\0';
        long val = strtol(octet_buf, NULL, 10);
        if (val < 0 || val > 255)
            throw Error::Host_ip();
        if (octet < 3)
        {
            if (i == Conf_File::tokens[index + 1].size() || Conf_File::tokens[index + 1][i] != '.')
                throw Error::Host_ip();
            i++;
            dot_count++;
        }
    }
    if (dot_count != 3)
        throw Error::Host_ip();
    index += 3;
    // Conf_File::Servers[server_index].host = line.substr(start, end - start);
}

void parse_index(size_t &index)
{
    size_t i = 0;
    if (index + 2 >= Conf_File::tokens.size())
        throw Error::Index();
    while (i < Conf_File::tokens.size() && !isKnownDirective(Conf_File::tokens[index]) && Conf_File::tokens[index] != ";")
    {
        Conf_File::Servers[server_index].index_files[i] = Conf_File::tokens[index];
        i++;
        index++;
    }
    if (index >= Conf_File::tokens.size())
        throw Error::UnexpectedEndOfFile();
    index += 3;
}

void parse_location_index(size_t &index)
{
    size_t i = 0;
    if (index + 2 >= Conf_File::tokens.size())
        throw Error::Index();
    while (i < Conf_File::tokens.size() && !isKnownDirective(Conf_File::tokens[index]) && Conf_File::tokens[index] != ";")
    {
        Conf_File::Servers[server_index].location.index_files[i] = Conf_File::tokens[index];
        i++;
        index++;
    }
    if (index >= Conf_File::tokens.size())
        throw Error::UnexpectedEndOfFile();
    index += 3;
}

void parse_server_name(size_t &index)
{
    if (index + 1 >= Conf_File::tokens.size() || index + 2 >= Conf_File::tokens.size())
        throw Error::Server_name();
    if (Conf_File::tokens[index + 2] != ";")
        throw Error::Server_name();
    if (Conf_File::tokens[index + 2] == ";")
        Conf_File::Servers[server_index].server_name = next_token(Conf_File::tokens, index);
    index += 2;
}

void parse_location(size_t &index)
{
    while (index < Conf_File::tokens.size() && Conf_File::tokens[index] != "{")
        index++;
    index++;
    while (Conf_File::tokens[index] != "}")
    {

    }
}

void parse_max_body_size(size_t &index)
{
    // size_t i = 0;
    size_t size = Conf_File::tokens[index + 1].size() - 1;
    if (index + 1 >= Conf_File::tokens.size() || index + 2 >= Conf_File::tokens.size())
        throw Error::MaxUploads();
    if (Conf_File::tokens[index + 2] != ";")
        throw Error::MaxUploads();
    if (Conf_File::tokens[index + 2] == ";")
    {
        Conf_File::Servers[server_index].max_body_size = strtol(next_token(Conf_File::tokens, index).substr(0, size - 1).c_str(), NULL, 10);
        if (max_uploads_is_unit(size, index))
        {
            if (Conf_File::tokens[index + 1][size - 1] == 'M')
                Conf_File::Servers[server_index].body_size_is_MB = true;
            if (Conf_File::tokens[index + 1][size - 1] == 'G')
                Conf_File::Servers[server_index].body_size_is_GB = true;
            if (Conf_File::tokens[index + 1][size - 1] == 'K')
                Conf_File::Servers[server_index].body_size_is_KB = true;
        }
    }
    index += 2;
}

void parse_listen(size_t &index)
{
    // size_t i = 0;
    if (index + 2 >= Conf_File::tokens.size())
        throw Error::Listen_port();
    if (Conf_File::tokens[index + 2] != ";")
        throw Error::Listen_port();
    char* endptr;
    long port = strtol(next_token(Conf_File::tokens, index).c_str(), &endptr, 10);
    // std::cout << index << std::endl;
    // exit(1);
    index += 2;
    // std::cout << index << std::endl;
    // exit(1);   
    if (*endptr != '\0')
        throw Error::Listen_port();
    if (port < 1 || port > 65535)
        throw Error::Listen_port();
    // std::cout << port << std::endl;
    
    Conf_File::Servers[server_index].listen_port = port;
}
void parse_error_pages(size_t &index)
{
    size_t i = 0;
    int code = 0;
    if (index + 2 >= Conf_File::tokens.size())
        throw Error::Error_page();
    char* endptr = NULL;
    size_t tmp = index;
    std::string buffer;
    index++;
    while (i < Conf_File::tokens.size() && Conf_File::tokens[tmp] != ";")
    {
        i++;
        tmp++;
    }
    if (Conf_File::tokens[tmp] == ";")
        buffer = Conf_File::tokens[tmp - 1];
    i = 0;
    // std::cout << "heres the first token after directive : " << Conf_File::tokens[index] << std::endl;
    while (i < Conf_File::tokens.size() && isdigit(Conf_File::tokens[index][0]) && index < tmp)
    {
        code = strtol(Conf_File::tokens[index].c_str(), &endptr, 0);
        if (code < 400 || code > 599)
            throw std::runtime_error("Error\nError page code is out of range!. 400 < 'error code' < 599");
        if (*endptr != '\0')
            throw Error::Error_page();
        Conf_File::Servers[server_index].error_pages[code] = buffer;
        i++;
        index++;
    }
    if (Conf_File::tokens[index][0] != '/')
        throw Error::Error_page();
    // else
        // Conf_File::Servers[server_index].error_pages[]
    // std::cout << "here is the recent error token : " << Conf_File::tokens[index] << std::endl;
    if (index >= Conf_File::tokens.size())
        throw Error::UnexpectedEndOfFile();
    index += 2;
}

void parse_directives(std::string& token, size_t &i)
{
    if (token == "host")
        parse_host(i);
    else if (token == "index")
        parse_index(i);
    else if (token == "root")
        parse_root(i);
    else if (token == "client_max_body_size")
        parse_max_body_size(i);
    else if (token == "server_name")
        parse_server_name(i);
    else if (token == "listen")
        parse_listen(i);
    else if (token == "error_page")
        parse_error_pages(i);
    // else if (token == "cgi_path")
    //     parse_cgi_path(i);
    else if (token == "return")
        parse_return(i);
    // else if (token == "location")
    // {
    //     i++;
    //     return ;
    // }
}

void parse_root_path(size_t &index)
{
    if (index + 2 >= Conf_File::tokens.size())
        throw Error::Root();
    if (Conf_File::tokens[index + 2] !=  ";")
        throw Error::SemiColon();
    if (path_file_exists(Conf_File::tokens[index + 1]))
        throw std::runtime_error(Conf_File::tokens[index + 1] + " : No such File or Directory!.");
    Conf_File::Servers[server_index].location.root = next_token(Conf_File::tokens, index);
    index += 2;
}

void parse_autoindex(size_t &index)
{
    if (index + 2 >= Conf_File::tokens.size())
        throw Error::Root();
    if (Conf_File::tokens[index + 2] !=  ";")
        throw Error::SemiColon();
    if (!is_autoindex_id(Conf_File::tokens[index + 1]))
        throw Error::Unkonwn_Directive_value();
    Conf_File::Servers[server_index].location.autoindex = next_token(Conf_File::tokens, index);
    index += 2;
}


void parse_upload_store(size_t &index)
{
    if (index + 2 >= Conf_File::tokens.size())
        throw Error::Root();
    if (Conf_File::tokens[index + 2] !=  ";")
        throw Error::SemiColon();
    if (path_file_exists(Conf_File::tokens[index + 1]))
    {
        if (mkdir(Conf_File::tokens[index + 1].c_str(), 777) != 0)
            throw std::runtime_error("Could not create the path : " + Conf_File::tokens[index + 1]);
    }
    Conf_File::Servers[server_index].location.upload_path = next_token(Conf_File::tokens, index);
    index += 2;
}

void parse_methods(size_t &index)
{
    size_t i = 0;
    if (index + 2 >= Conf_File::tokens.size())
        throw Error::Methods();
    index++;
    while (i < Conf_File::tokens.size() && Conf_File::tokens[index] != ";")
    {
        if (!is_http_method(Conf_File::tokens[index]))
            throw Error::Unkonwn_Directive_value();
        Conf_File::Servers[server_index].location.allowed_methods.push_back(next_token(Conf_File::tokens, index));
        i++;
    }
    if (index >= Conf_File::tokens.size())
        throw Error::UnexpectedEndOfFile();
    index++;
}

void parse_cgi_extension(size_t &index)
{
    if (index + 2 >= Conf_File::tokens.size() || Conf_File::tokens[index + 1][0] != '.')
        throw Error::CGI_Extension();
    if (Conf_File::tokens[index + 2] !=  ";")
        throw Error::SemiColon();
    Conf_File::Servers[server_index].location.cgi_extension = next_token(Conf_File::tokens, index);
    index += 2;
}

void parse_cgi_path(size_t &index)
{
    if (index + 2 >= Conf_File::tokens.size())
        throw Error::CGI_Extension();
    if (Conf_File::tokens[index + 2] !=  ";")
        throw Error::SemiColon();
    if (!path_file_exists(Conf_File::tokens[index + 1]))
        throw Error::CGI_Path();
    Conf_File::Servers[server_index].location.cgi_path = next_token(Conf_File::tokens, index);
    index += 2;
}


void parse_location_directives(std::string& token, size_t &i)
{
    if (token == "root")
        parse_root_path(i);
    else if (token == "index")
        parse_location_index(i);
    else if (token == "autoindex")
        parse_autoindex(i);
    else if (token == "error_page")
        parse_error_pages(i);
    else if (token == "return")
        parse_return(i);
    else if (token == "allowed_methods")
        parse_methods(i);
    else if (token == "cgi_extension")
        parse_cgi_extension(i);
    else if (token == "upload_store")
        parse_upload_store(i);
    else 
        throw Error::Unknown_Directive();
}


void parse_config_file()
{
    size_t i = 0;
    size_t server_index = 0;
    bool in_server = false;
    int depth = 0;
    while (i < Conf_File::tokens.size())
    {
        std::string token = Conf_File::tokens[i];
        if (token == "server")
        {
            Server_block new_server;
            Conf_File::Servers.push_back(new_server);
            server_index = (Conf_File::Servers.size() -1);
            // std::cout << "reached 2 \n";
            if (depth > 0)
                throw std::runtime_error("Error\n'server' block cannot be nested inside another block!.");
            if (i + 1 >= Conf_File::tokens.size() || Conf_File::tokens[i + 1] != "{")
                throw std::runtime_error("Error\nExpected '{' right after 'server'");
            in_server = true;
            depth++;
            i += 2;
        }
        else if (token == "location")
        {
            if (!in_server)
                throw std::runtime_error("Error\n'location' directive cannot be nested outside 'server'!.");
            if (i + 2 >= Conf_File::tokens.size() || Conf_File::tokens[i + 2] != "{")
                throw std::runtime_error("Error\nExpected '{' after 'location <path>'!.");
            Conf_File::Servers[server_index].location.path = next_token(Conf_File::tokens, i);
            i += 2;
            // std::cout << "here's the location path : " << Conf_File::tokens[i] << std::endl;
            while (i < Conf_File::tokens.size() && Conf_File::tokens[i] != "}")
            {
                // std::cout << Conf_File::tokens[i] << std::endl;
                parse_location_directives(Conf_File::tokens[i], i);
            }
            // depth++;
            i++;  
        }
        else if (token == "{")
            throw std::runtime_error("Error\nUnexpected '{'. only 'server' or 'location' can open a block!.");
        else if (token == "}")
        {
            depth--;
            if (depth < 0)
                throw std::runtime_error("Error\nUnbalanced '}'. too many closing braces!.");
            if (depth == 0)
                in_server = false;
            i++;
        }
        else if (token != ";")
        {
            if (isKnownDirective(token))
            {
                if (!in_server)
                    throw std::runtime_error("Error\nDirective '" + token + "' found outside 'server' block!.");
            }
        }
        usleep(110000);
        parse_directives(token, i);
        // std::cout << "reached\n";
        // std::cout << token << std::endl;
        // std::cout << i << std::endl;
    }
    if (depth != 0)
        throw std::runtime_error("Error\nUnclosed block at the end of the file!.");
}


// void parse_file()
// {
//     size_t i = 0;
//     while (i < Conf_File::rawLines.size())
//     {
//         size_t pos = Conf_File::rawLines[i].find("server");
//         if (pos != std::string::npos)
//         {
            
//         }
//     }
// }