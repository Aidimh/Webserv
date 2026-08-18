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

void parse_return(size_t &index)
{

    // std::cout << Conf_File::tokens[index] << " " << Conf_File::tokens[index + 1] << " " << Conf_File::tokens[index + 2] << std::endl;
    size_t count = Conf_File::Servers[server_index].location_count;
    size_t values_count = 0;
    while(values_count < Conf_File::tokens.size() && Conf_File::tokens[index + values_count] != ";")
        values_count++;
    values_count -= 1; // Subtract 1 to exclude the "return" token itself
    if (values_count == 2 && Conf_File::tokens[index + 3] != ";")
        throw Error::Return();
    else if (values_count == 1 && Conf_File::tokens[index + 2] == ";")
        throw Error::Return();
    if (values_count != 2 && values_count != 1)
        throw Error::Return();
    if (values_count == 1 && (index + 1 < Conf_File::tokens.size()))
    {
        if(Conf_File::tokens[index + 1].find("https://") != std::string::npos)
        {
            Conf_File::Servers[server_index].location[Conf_File::Servers[server_index].location_count].return_value_is_URL_only = true;
            Conf_File::Servers[server_index].location[Conf_File::Servers[server_index].location_count].return_value_is_code_only = false;
            Conf_File::Servers[server_index].location[Conf_File::Servers[server_index].location_count].return_url = next_token(Conf_File::tokens, index);
        }
        else if (is_number(Conf_File::tokens[index + 1].c_str()))
        {
            Conf_File::Servers[server_index].location[Conf_File::Servers[server_index].location_count].return_value_is_URL_only = false;
            Conf_File::Servers[server_index].location[Conf_File::Servers[server_index].location_count].return_value_is_code_only = true;
            char *garbage = NULL;
            Conf_File::Servers[server_index].location[Conf_File::Servers[server_index].location_count].return_code = strtol(next_token(Conf_File::tokens, index).c_str(), &garbage, 10);
            if (garbage != NULL && *garbage != '\0')
                throw Error::Return();
        }
        else
            throw Error::Return();
        index += 3;
    }
    else if (values_count == 2 && (index + 2 < Conf_File::tokens.size()))
    {
        if (!is_number(Conf_File::tokens[index + 1].c_str()))
            throw Error::Return();
        char *garbage = NULL;
        size_t code = strtol(Conf_File::tokens[index + 1].c_str(), &garbage, 10);
        if (code < 100 || code > 599 || (garbage != NULL && *garbage != '\0'))
            throw Error::Return();

        if ((Conf_File::tokens[index + 2].find("https://") != std::string::npos || Conf_File::tokens[index + 2].find("http://") != std::string::npos) && Conf_File::tokens[index + 2][0] != '/')
        {
            std::cout << "reached here\n";
            Conf_File::Servers[server_index].location[count].has_code_and_url = true;
            char* garbage = NULL;
            Conf_File::Servers[server_index].location[count].return_code_and_url[code] = Conf_File::tokens[index + 2];
            if (garbage != NULL && *garbage != '\0')
                throw Error::Return(); 
        }
        else if (Conf_File::tokens[index + 2][0] == '/')
        {
            Conf_File::Servers[server_index].location[count].has_code_and_path = true;
            char* garbage = NULL;
            Conf_File::Servers[server_index].location[count].return_code_and_path[code] = Conf_File::tokens[index + 2];
            if (garbage != NULL && *garbage != '\0')
                throw Error::Return(); 
        }
        else if (Conf_File::tokens[index + 2][0] == '\"')
        {
            Conf_File::Servers[server_index].location[count].has_code_and_message = true;
            Conf_File::Servers[server_index].location[count].return_code_and_message[code] = Conf_File::tokens[index + 2];
            // std::string &msg = Conf_File::Servers[server_index].location[count].return_code_and_message[code];
            // if (!msg.empty() && msg[msg.length() - 1] != '"')
            //     throw Error::Return();
        }
        index += 4;
    }
    // std::cout << Conf_File::tokens[index] << std::endl;
    std::cout << "parse_return: parsed return directive for location=" << count
              << " server=" << server_index
              << " return_value_is_URL_only=" << Conf_File::Servers[server_index].location[count].return_value_is_URL_only
              << " return_value_is_code_only=" << Conf_File::Servers[server_index].location[count].return_value_is_code_only
              << " has_code_and_url=" << Conf_File::Servers[server_index].location[count].has_code_and_url
              << " has_code_and_path=" << Conf_File::Servers[server_index].location[count].has_code_and_path
              << " has_code_and_message=" << Conf_File::Servers[server_index].location[count].has_code_and_message << std::endl;
    // exit(0); // Exit after printing the debug message
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

void parse_location_max(size_t &index)
{
    size_t size = Conf_File::tokens[index + 1].size();
    if (index + 2 >= Conf_File::tokens.size() || Conf_File::tokens[index + 2] != ";")
        throw Error::MaxUploads();

    size_t count = Conf_File::Servers[server_index].location_count;
    // Location_Config& location = Conf_File::Servers[server_index].location[count];
    char *unit = NULL;
    Conf_File::Servers[server_index].location[count].max_body_size = strtol(next_token(Conf_File::tokens, index).substr(0, size).c_str(), &unit, 10);
    if (unit != NULL && *unit != '\0')
    {
        if ((unit[0] == 'B' || unit[0] == 'b') && unit[1] == '\0'){}
        else if ((unit[0] == 'K' || unit[0] == 'k') && unit[1] == '\0')
        {
            Conf_File::Servers[server_index].location[count].max_body_size *= 1024;
        }
        else if ((unit[0] == 'M' || unit[0] == 'm') && unit[1] == '\0')
        {
            Conf_File::Servers[server_index].location[count].max_body_size *= 1024 * 1024;
        }
        else if ((unit[0] == 'G' || unit[0] == 'g') && unit[1] == '\0') {
            Conf_File::Servers[server_index].location[count].max_body_size *= 1024 * 1024 * 1024;
        }
        else
            throw Error::MaxUploads();
    }
    index += 2;
    Conf_File::Servers[server_index].location[count].has_max_body_size = true;
    DEBUG("ConfFile") << "parse_max_body_size: parsed client_max_body_size="
                      << Conf_File::Servers[server_index].max_body_size
                      << " bytes server=" << server_index;
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