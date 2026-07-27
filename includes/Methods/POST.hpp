#ifndef POST_HPP
#define POST_HPP

#include "AMethod.hpp"
#include "Response.hpp"
#include "PathType.hpp"
#include "HttpRequest.hpp"
#include "MultipartUploadStrategy.hpp"
#include <sys/stat.h>
#include <fstream>
#include <unistd.h>


class POST : public AMethod
{
    private:
        MultipartUploadStrategy multiPart;
        Response buildCreatedResponse(int statusCode,const std::string& reasonPhrase) const;
        std::string getParentDirectory(const std::string& target) const;
        bool canWrite(const std::string& path) const;
        bool validateParentDirectory(const std::string& target) const;
        bool saveBody(const std::string& path, const std::string& body) const;
        bool isMultipartRequest(const HttpRequest& request) const;
        bool isRequestValid(const HttpRequest& request) const;
    public:
        POST();
        virtual ~POST();
        Response handleMultipartRequest(const HttpRequest& request,const std::string& target);
        Response handleRegularRequest(const HttpRequest& request,const std::string& target);
        virtual Response execute(const HttpRequest& request,const Server_block& server,const Location_Config* location);
};

#endif
