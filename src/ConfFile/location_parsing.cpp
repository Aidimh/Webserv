#include "../../includes/multiplexing/header.hpp"
#include "../Logging/Logging.hpp"

extern int server_index;

// void parse_root_path(size_t &index)
// {
//     if (index + 2 >= Conf_File::tokens.size())
//         throw Error::Root();
//     if (Conf_File::tokens[index + 2] !=  ";")
//         throw Error::SemiColon();
//     if (!path_file_exists(Conf_File::tokens[index + 1]))
//         throw std::runtime_error(Conf_File::tokens[index + 1] + " : No such File or Directory!.");
//     Conf_File::Servers[server_index].location[Conf_File::Servers[server_index].location_count].root = next_token(Conf_File::tokens, index);
//     index += 2;
// } 


void parse_root_path(size_t &index)
{
    if (index + 2 >= Conf_File::tokens.size())
        throw Error::Root();

    if (Conf_File::tokens[index + 2] != ";")
        throw Error::SemiColon();

    if (!path_file_exists(Conf_File::tokens[index + 1]))
        throw std::runtime_error(Conf_File::tokens[index + 1] + " : No such File or Directory!.");
    size_t count = Conf_File::Servers[server_index].location_count;
    Conf_File::Servers[server_index].location[count].root = next_token(Conf_File::tokens, index);
    Conf_File::Servers[server_index].location[count].has_root = true;
    index += 2;
    DEBUG("ConfFile") << "parse_root_path: parsed root=" << Conf_File::Servers[server_index].location[count].root
                      << " location=" << count << " server=" << server_index;
}

void parse_autoindex(size_t &index)
{
    if (index + 2 >= Conf_File::tokens.size())
        throw Error::Root();
    if (Conf_File::tokens[index + 2] !=  ";")
        throw Error::SemiColon();
    if (!is_autoindex_id(Conf_File::tokens[index + 1]))
        throw Error::Unknown_Directive_value();
    Conf_File::Servers[server_index].location[Conf_File::Servers[server_index].location_count].autoindex = next_token(Conf_File::tokens, index);
    Conf_File::Servers[server_index].location[Conf_File::Servers[server_index].location_count].has_autoindex = true;
    index += 2;
    DEBUG("ConfFile") << "parse_autoindex: parsed autoindex="
                      << Conf_File::Servers[server_index].location[Conf_File::Servers[server_index].location_count].autoindex
                      << " location=" << Conf_File::Servers[server_index].location_count
                      << " server=" << server_index;
}

void parse_upload_store(size_t &index)
{
    if (index + 2 >= Conf_File::tokens.size())
        throw Error::Root();
    if (Conf_File::tokens[index + 2] !=  ";")
        throw Error::SemiColon();
    if (!path_file_exists(Conf_File::tokens[index + 1]))
    {
        DEBUG("ConfFile") << "parse_upload_store: mkdir " << Conf_File::tokens[index + 1];
        if (mkdir(Conf_File::tokens[index + 1].c_str(), 777) != 0)
        {
            DEBUG("ConfFile") << "parse_upload_store: mkdir failed path=" << Conf_File::tokens[index + 1]
                              << ": " << strerror(errno);
            throw std::runtime_error("Could not create the path : " + Conf_File::tokens[index + 1]);
        }
    }
    Conf_File::Servers[server_index].location[Conf_File::Servers[server_index].location_count].upload_path = next_token(Conf_File::tokens, index);
    index += 2;
    DEBUG("ConfFile") << "parse_upload_store: parsed upload_store="
                      << Conf_File::Servers[server_index].location[Conf_File::Servers[server_index].location_count].upload_path
                      << " location=" << Conf_File::Servers[server_index].location_count
                      << " server=" << server_index;
}

void parse_methods(size_t &index)
{
    size_t i = 0;
    if (index + 2 >= Conf_File::tokens.size())
        throw Error::Methods();
    index++;
    // std::cout << "reached\n";
    // std::cout << next_token(Conf_File::tokens, index);
    while (i < Conf_File::tokens.size() && Conf_File::tokens[index] != ";")
    {
        if (!is_http_method(Conf_File::tokens[index]))
            throw Error::Unknown_Directive_value();
        Conf_File::Servers[server_index].location[Conf_File::Servers[server_index].location_count].allowed_methods.push_back(Conf_File::tokens[index++]);
        i++;
    }
    if (index >= Conf_File::tokens.size())
        throw Error::UnexpectedEndOfFile();
    index++;
    DEBUG("ConfFile") << "parse_methods: parsed allowed_methods count="
                      << Conf_File::Servers[server_index].location[Conf_File::Servers[server_index].location_count].allowed_methods.size()
                      << " location=" << Conf_File::Servers[server_index].location_count
                      << " server=" << server_index;
}

void parse_cgi_extension(size_t &index)
{
    size_t j = Conf_File::Servers[server_index].location[Conf_File::Servers[server_index].location_count].cgi_paths_index;
    size_t i = Conf_File::Servers[server_index].location[Conf_File::Servers[server_index].location_count].cgi_extns_index;
    if (index + 3 >= Conf_File::tokens.size() || !is_cgi_extension(Conf_File::tokens[index + 1]))
        throw Error::CGI_Extension();
    if (Conf_File::tokens[index + 2] == ";")
        throw Error::CGI_Path();
    if (Conf_File::tokens[index + 3] !=  ";")
        throw Error::SemiColon();
    Conf_File::Servers[server_index].location[Conf_File::Servers[server_index].location_count].cgi_extensions.push_back(next_token(Conf_File::tokens, index));
    i++;
    Conf_File::Servers[server_index].location[Conf_File::Servers[server_index].location_count].cgi_paths.push_back(next_token(Conf_File::tokens, index));
    if (!path_file_exists(Conf_File::Servers[server_index].location[Conf_File::Servers[server_index].location_count].cgi_paths[j]))
        throw Error::CGI_Path();
    j++;
    Conf_File::Servers[server_index].location[Conf_File::Servers[server_index].location_count].cgi_paths_index = j;
    Conf_File::Servers[server_index].location[Conf_File::Servers[server_index].location_count].cgi_extns_index = i;
    if (i != j)
        throw Error::CGI_Path();
    index += 2;
    DEBUG("ConfFile") << "parse_cgi_extension: parsed cgi extension="
                      << Conf_File::Servers[server_index].location[Conf_File::Servers[server_index].location_count].cgi_extensions.back()
                      << " path=" << Conf_File::Servers[server_index].location[Conf_File::Servers[server_index].location_count].cgi_paths.back()
                      << " location=" << Conf_File::Servers[server_index].location_count
                      << " server=" << server_index;
}

// void parse_cgi_path(size_t &index)
// {
//     if (index + 2 >= Conf_File::tokens.size())
//         throw Error::CGI_Extension();
//     if (Conf_File::tokens[index + 2] !=  ";")
//         throw Error::SemiColon();
//     if (!path_file_exists(Conf_File::tokens[index + 1]))
//         throw Error::CGI_Path();
//     Conf_File::Servers[server_index].location.cgi_path = next_token(Conf_File::tokens, index);
//     index += 2;
// }

void parse_return(size_t &index)
{
    // size_t i = 0;
    if (index + 2 >= Conf_File::tokens.size())
        throw Error::Unknown_Directive();
    if (Conf_File::tokens[index + 2] != ";")
        throw Error::Unknown_Directive();
    //todo: you should store the return value and the status code in the location config, and go to the next token
    WARN() << "parse_return: 'return' directive is validated but not stored, it will have no effect";
}