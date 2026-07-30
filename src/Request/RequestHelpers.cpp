#include "../../includes/multiplexing/header.hpp"
#include "../../includes/Request/ClientRequest.hpp"

bool ValidLine(std::string line)
{
    for (size_t i = 0; i < line.length(); i++)
    {
        if (line[i] != '\n' && line[i] != '\r' && line[i] != ' ')
            return true;
    }
    return false;
}

size_t removeWhitespace(Client& client)
{
    size_t i;

    i = 0;
    while (i < client.request.length() && (client.request[i] == ' ' || client.request[i] == '\n' || client.request[i] == '\r' || client.request[i] == '\t'))
    {
        i++;
    }
    return (i);
}

void MyToLower(std::string &str)
{
    size_t i = 0;
    while (i < str.length())
    {
        str[i] = ::tolower(str[i]);
        i++;
    }
}

std::string	RemoveFirstLastSpaces(std::string& line)
{
    size_t  begin;
    size_t  finish;

    begin = line.find_first_not_of(" \n\r\t");
    finish = line.find_last_not_of(" \n\r\t");

    if (begin == std::string::npos)
        return ("");
    return (line.substr(begin, finish - begin + 1));
}
