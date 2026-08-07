#ifndef AMETHOD_HPP
#define AMETHOD_HPP

#include "Response.hpp"
#include "../Response/PathType.hpp"
#include "../Routing/Router.hpp"
#include "../multiplexing/header.hpp"

#include <iostream>
#include <cstdio>
#include <unistd.h>
#include <sys/stat.h>



class AMethod
{
    public:
        virtual ~AMethod();
        virtual Response execute(Client& client, const Server_block& server) = 0;
        static Response buildErrorResponse(short status,const std::string& message);
    protected:
        std::string urlDecode(const std::string& url) const;
        std::string normalizePath(const std::string& path, bool& outOfBounds)const;
        const Location_Config* resolveLocation(const Client& client, const Server_block& server) const;
        std::string resolveTarget(const Client& client, const Server_block& server, const Location_Config* location) const;
        PathType getPathType(const std::string& path) const;
        bool fileExists(const std::string& path) const;
        bool isDirectory(const std::string& path) const;
};

#endif
