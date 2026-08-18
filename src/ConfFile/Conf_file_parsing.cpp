#include "header.hpp"
#include "Error.hpp"
#include "../Logging/Logging.hpp"

extern int server_index;



void parse_root(size_t &index)
{
    if (index + 2 >= Conf_File::tokens.size())
        throw Error::Root();
    if (Conf_File::tokens[index + 2] != ";")
        throw Error::Root();
    Conf_File::Servers[server_index].root = next_token(Conf_File::tokens, index);
    if (!path_file_exists(Conf_File::Servers[server_index].root))
        throw Error::Root();
    index += 2;
    Conf_File::Servers[server_index].root_found = true;
    DEBUG("ConfFile") << "parse_root: parsed root=" << Conf_File::Servers[server_index].root
                      << " server=" << server_index;
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
    Conf_File::Servers[server_index].host = Conf_File::tokens[index + 1];
    index += 3;
    Conf_File::Servers[server_index].host_found = true;
    DEBUG("ConfFile") << "parse_host: parsed host=" << Conf_File::Servers[server_index].host
                      << " server=" << server_index;
}

void parse_index(size_t &index)
{
    size_t i = 0;
    if (index + 2 >= Conf_File::tokens.size())
        throw Error::Index();
    index++;
    while (i < Conf_File::tokens.size() && Conf_File::tokens[index] != ";")
    {
        Conf_File::Servers[server_index].index_files.push_back(Conf_File::tokens[index]);
        i++;
        index++;
    }
    if (index >= Conf_File::tokens.size())
        throw Error::UnexpectedEndOfFile();
    index += 1;
    Conf_File::Servers[server_index].index_found = true;
    DEBUG("ConfFile") << "parse_index: parsed index files count="
                      << Conf_File::Servers[server_index].index_files.size()
                      << " server=" << server_index;
}

void parse_location_index(size_t &index)
{
    size_t i = 0;
    if (index + 2 >= Conf_File::tokens.size())
        throw Error::Index();
    index++;
    while (i < Conf_File::tokens.size() && Conf_File::tokens[index] != ";")
    {
        Conf_File::Servers[server_index].location[Conf_File::Servers[server_index].location_count].index_files.push_back(Conf_File::tokens[index]);
        i++;
        index++;
    }
    if (index >= Conf_File::tokens.size())
        throw Error::UnexpectedEndOfFile();
    index++;
    Conf_File::Servers[server_index].location[Conf_File::Servers[server_index].location_count].has_index = true;
    Conf_File::Servers[server_index].location_found = true;
    DEBUG("ConfFile") << "parse_location_index: parsed index files count="
                      << Conf_File::Servers[server_index].location[Conf_File::Servers[server_index].location_count].index_files.size()
                      << " location=" << Conf_File::Servers[server_index].location_count
                      << " server=" << server_index;
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
    Conf_File::Servers[server_index].server_name_found = true;
    DEBUG("ConfFile") << "parse_server_name: parsed server_name="
                      << Conf_File::Servers[server_index].server_name
                      << " server=" << server_index;
}

void parse_max_body_size(size_t &index)
{
    size_t size = Conf_File::tokens[index + 1].size();
    if (index + 1 >= Conf_File::tokens.size() || index + 2 >= Conf_File::tokens.size())
        throw Error::MaxUploads();
    if (Conf_File::tokens[index + 2] != ";")
        throw Error::MaxUploads();
    char *unit = NULL;
    if (Conf_File::tokens[index + 2] == ";")
    {
        Conf_File::Servers[server_index].max_body_size = strtol(next_token(Conf_File::tokens, index).substr(0, size).c_str(), &unit, 10);
        if (unit != NULL && *unit != '\0')
        {
            if ((unit[0] == 'B' || unit[0] == 'b') && unit[1] == '\0'){}
            else if ((unit[0] == 'K' || unit[0] == 'k') && unit[1] == '\0')
            {
                Conf_File::Servers[server_index].max_body_size *= 1024;
            }
            else if ((unit[0] == 'M' || unit[0] == 'm') && unit[1] == '\0')
            {
                Conf_File::Servers[server_index].max_body_size *= 1024 * 1024;
            }
            else if ((unit[0] == 'G' || unit[0] == 'g') && unit[1] == '\0') {
                Conf_File::Servers[server_index].max_body_size *= 1024 * 1024 * 1024;
            }
            else
                throw Error::MaxUploads();
        }
    }
    index += 2;
    Conf_File::Servers[server_index].client_max_body_found = true;
    DEBUG("ConfFile") << "parse_max_body_size: parsed client_max_body_size="
                      << Conf_File::Servers[server_index].max_body_size
                      << " bytes server=" << server_index;
}

void parse_listen(size_t &index)
{
    if (index + 2 >= Conf_File::tokens.size())
        throw Error::Listen_port();
    if (Conf_File::tokens[index + 2] != ";")
        throw Error::Listen_port();
    char* endptr;
    Conf_File::Servers[server_index].listen_port_str.push_back(Conf_File::tokens[index + 1]);
    long port = strtol(next_token(Conf_File::tokens, index).c_str(), &endptr, 10);
    index += 2;
    if (*endptr != '\0')
        throw Error::Listen_port();
    if (port < 1 || port > 65535)
        throw Error::Listen_port();
    
    Conf_File::Servers[server_index].listen_port.push_back(port);
    Conf_File::Servers[server_index].listen_found = true;
    Conf_File::Servers[server_index].ports_count++;
    DEBUG("ConfFile") << "parse_listen: parsed listen port=" << port
                      << " server=" << server_index
                      << " ports_count=" << Conf_File::Servers[server_index].ports_count;
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
    while (i < Conf_File::tokens.size() && isdigit(Conf_File::tokens[index][0]) && index < tmp)
    {
        code = strtol(Conf_File::tokens[index].c_str(), &endptr, 0);
        if (code < 400 || code > 599)
            throw std::runtime_error("Error\nError page code is out of range!. 400 < 'error code' < 599");
        if (*endptr != '\0')
            throw Error::Error_page();
        Conf_File::Servers[server_index].error_pages[code] = buffer;
        DEBUG("ConfFile") << "parse_error_pages: parsed error_page code=" << code
                          << " path=" << buffer << " server=" << server_index;
        i++;
        index++;
    }
    if (Conf_File::tokens[index][0] != '/')
        throw Error::Error_page();
    if (index >= Conf_File::tokens.size())
        throw Error::UnexpectedEndOfFile();
    index += 2;
}

void parse_server_autoindex(size_t &index)
{
    if (index + 2 >= Conf_File::tokens.size())
        throw Error::Root();
    if (Conf_File::tokens[index + 2] !=  ";")
        throw Error::SemiColon();
    if (!is_autoindex_id(Conf_File::tokens[index + 1]))
        throw Error::Unknown_Directive_value();
    Conf_File::Servers[server_index].server_auto_index = next_token(Conf_File::tokens, index);
    Conf_File::Servers[server_index].server_has_autoindex = true;
    index += 2;
    DEBUG("ConfFile") << "parse_server_autoindex: parsed autoindex="
                      << Conf_File::Servers[server_index].server_auto_index
                      << " server=" << server_index;
}


void parse_directives(std::string& token, size_t &i)
{
    DDEBUG("ConfFile") << "parse_directives: dispatching token=" << token << " index=" << i;
    if (token == "host")
        parse_host(i);
    else if (token == "index")
        parse_index(i);
    else if (token == "root")
        parse_root(i);
    else if (token == "client_max_body_size")
        parse_max_body_size(i);
    else if (token == "autoindex")
        parse_server_autoindex(i);
    else if (token == "server_name")
        parse_server_name(i);
    else if (token == "listen")
        parse_listen(i);
    else if (token == "error_page")
        parse_error_pages(i);
    else if (token == "return")
        parse_return(i);
    else if (token == "location" || token == "{" || token == "}" || token == "server")
        return;
    else
        throw Error::Unknown_Directive();
}


void parse_location_directives(std::string& token, size_t &i)
{
    DDEBUG("ConfFile") << "parse_location_directives: dispatching token=" << token << " index=" << i;
    if (token == "root")
        parse_root_path(i);
    else if (token == "index")
        parse_location_index(i);
    else if (token == "client_max_body_size")
        parse_location_max(i);
    else if (token == "index")
        parse_location_index(i);
    else if (token == "autoindex")
        parse_autoindex(i);
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
    server_index = -1;
    bool in_server = false;
    int depth = 0;
    DEBUG("ConfFile") << "parse_config_file: parsing " << Conf_File::tokens.size() << " tokens";
    while (i < Conf_File::tokens.size())
    {
        std::string token = Conf_File::tokens[i];
        if (token == "server")
        {
            server_index++;
            Server_block new_server;
            Conf_File::Servers.push_back(new_server);
            Conf_File::Servers[server_index].location_count = -1;
            if (depth > 0)
                throw std::runtime_error("Error\n'server' block cannot be nested inside another block!.");
            if (i + 1 >= Conf_File::tokens.size() || Conf_File::tokens[i + 1] != "{")
                throw std::runtime_error("Error\nExpected '{' right after 'server'");
            in_server = true;
            Conf_File::Servers[server_index].server_found = true;
            depth++;
            i += 2;
            DEBUG("ConfFile") << "parse_config_file: entering server block index=" << server_index;
        }
        else if (token == "location")
        {
            Conf_File::Servers[server_index].location_count++;
            Conf_File::Servers[server_index].location_found = true;
            
            Location_Config obj;
            Conf_File::Servers[server_index].location.push_back(obj);
            Conf_File::Servers[server_index].location[Conf_File::Servers[server_index].location_count].cgi_extns_index = 0;
            Conf_File::Servers[server_index].location[Conf_File::Servers[server_index].location_count].cgi_paths_index = 0;
            if (!in_server)
                throw std::runtime_error("Error\n'location' directive cannot be nested outside 'server'!.");
            if (i + 2 >= Conf_File::tokens.size() || Conf_File::tokens[i + 2] != "{")
                throw std::runtime_error("Error\nExpected '{' after 'location <path>'!.");
            Conf_File::Servers[server_index].location[Conf_File::Servers[server_index].location_count].path = next_token(Conf_File::tokens, i);
            i += 2;
            DEBUG("ConfFile") << "parse_config_file: entering location block path="
                              << Conf_File::Servers[server_index].location[Conf_File::Servers[server_index].location_count].path
                              << " index=" << Conf_File::Servers[server_index].location_count
                              << " server=" << server_index;
            while (i < Conf_File::tokens.size() && Conf_File::tokens[i] != "}")
            {
                parse_location_directives(Conf_File::tokens[i], i);
            }
            i++;
            DEBUG("ConfFile") << "parse_config_file: leaving location block index="
                              << Conf_File::Servers[server_index].location_count
                              << " server=" << server_index << "hers the max body size of the location : " << Conf_File::Servers[server_index].location[Conf_File::Servers[server_index].location_count].max_body_size;
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
        parse_directives(token, i);
    }
    if (depth != 0)
        throw std::runtime_error("Error\nUnclosed block at the end of the file!.");
    INFO() << "parse_config_file: loaded " << Conf_File::Servers.size() << " server block(s)";
}


int every_server_has_listen_port()
{
    size_t i = 0;
    int error_nb = 1;
    while (i < Conf_File::Servers.size())
    {
        if (!Conf_File::Servers[i].listen_found)
        {
            ERR() << "every_server_has_listen_port: invalid config: server block "
                  << error_nb << " has no listen port";
            return error_nb;
        }
        i++;
        error_nb++;
    }
    DEBUG("ConfFile") << "every_server_has_listen_port: all " << Conf_File::Servers.size()
                      << " server block(s) have a listen port";
    return 0;
}
