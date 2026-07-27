#ifndef HttpRequest_hpp
#define HttpRequest_hpp

#include "ClientRequest.hpp"
#include <string>
#include <map>

class HttpRequest
{
    public:
        std::string method;
        std::string path;
        std::string version;
        std::map<std::string, std::string> headers;
        std::string body;
};

#endif

