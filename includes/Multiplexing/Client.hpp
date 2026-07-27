#ifndef CLIENT_HPP
#define CLIENT_HPP

#include <string>
#include "Request/ClientRequest.hpp"

struct Client
{
    int fd;
    std::string request;
    std::string response;
    int port;
    ClientRequest parsed_request;
};

#endif