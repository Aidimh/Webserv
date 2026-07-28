#ifndef AMETHOD_HPP
#define AMETHOD_HPP

#include "Response.hpp"
#include "HttpRequest.hpp"
#include "PathType.hpp"
#include "../multiplexing/header.hpp"
// #include "MultipartUploadStrategy.hpp"

#include <iostream>
#include <cstdio>
#include <unistd.h>
#include <sys/stat.h>

// struct Location_Config;
// class Server_block;

class AMethod
{
    public:
        virtual ~AMethod();
        virtual Response execute(Client& Client, const Server_block& server) = 0;
    protected:
        Response buildErrorResponse(int status,const std::string& message) const;
        std::string resolveTarget(const HttpRequest& request,const Server_block& server,const Location_Config* location) const;
        PathType getPathType(const std::string& path) const;
        bool fileExists(const std::string& path) const;
        bool isDirectory(const std::string& path) const;
};

#endif
