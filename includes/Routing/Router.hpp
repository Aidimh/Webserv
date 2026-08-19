#ifndef ROUTER_HPP
#define ROUTER_HPP

#include "../multiplexing/header.hpp"

class Router
{
    public:
        static bool isCGIRequest(const ClientRequest& request,const Location_Config& location);
        static const Location_Config* resolveLocation(const std::string& uri,const Server_block& server);
        static bool isMethodAllowed(const std::string& method,const Location_Config& location);
        static std::string allowedMethodList(const Location_Config& location);

    private:
        static bool matchesLocation(const std::string& uri,const std::string& locationPath);
};

#endif
