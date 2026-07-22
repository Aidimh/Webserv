// #include "../../header.hpp"
#include "../../include/request/RequestHelpers.hpp"

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
