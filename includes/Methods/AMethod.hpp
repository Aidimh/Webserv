#ifndef AMETHOD_HPP
#define AMETHOD_HPP

#include "Response.hpp"
#include "PathType.hpp"
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
    protected:
        Response buildErrorResponse(int status,const std::string& message) const;
        const Location_Config* resolveLocation(const Client& client, const Server_block& server) const;
        std::string resolveTarget(const Client& client, const Server_block& server, const Location_Config* location) const;
        PathType getPathType(const std::string& path) const;
        bool fileExists(const std::string& path) const;
        bool isDirectory(const std::string& path) const;
};

#endif
