#ifndef ROUTER_HPP
#define ROUTER_HPP

#include "../multiplexing/header.hpp"

/*
 * Router kayqelleb 3la l-location li katmatchi l-URI dyal request.
 * Ma kaydirch execution dyal GET/POST/DELETE; hadik mas2ouliya
 * dyal MethodFactory w l-method handler.
 */
class Router
{
    public:
        static const Location_Config* resolveLocation(const std::string& uri,const Server_block& server);
        static bool isMethodAllowed(const std::string& method,const Location_Config& location);

    private:
        static bool matchesLocation(const std::string& uri,const std::string& locationPath);
};

#endif
