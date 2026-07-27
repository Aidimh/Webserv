#ifndef AMETHOD_HPP
#define AMETHOD_HPP

#include "Response/Response.hpp"
#include "Request/HttpRequest.hpp"
#include "Core/ConfigTypes.hpp"
#include "Core/PathType.hpp"

#include <string>
#include <sstream> //ostringstream and oss
#include <sys/stat.h>


class AMethod
{
    public:
        virtual ~AMethod();
        virtual Response execute(const HttpRequest& request,const Server_block& server,const Location_Config* location) = 0;
    protected:
        Response buildErrorResponse(int status,const std::string& message) const;
        std::string resolveTarget(const HttpRequest& request,const Server_block& server,const Location_Config* location) const;
        PathType getPathType(const std::string& path) const;
        bool fileExists(const std::string& path) const;
        bool isDirectory(const std::string& path) const;
};

#endif
