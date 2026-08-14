#ifndef POST_HPP
#define POST_HPP

#include "AMethod.hpp"
#include "Response.hpp"
#include "PathType.hpp"
#include "MultipartUploadStrategy.hpp"
#include <sys/stat.h>
#include <fstream>
#include <unistd.h>


class POST : public AMethod
{
    private:
        MultipartUploadStrategy multiPart;
        // Response buildCreatedResponse(int statusCode,const std::string& reasonPhrase) const;
        std::string getParentDirectory(const std::string& target) const;
        bool canWrite(const std::string& path) const;
        PathType validateParentDirectory(const std::string& target) const;
        bool saveBody(const std::string& path, const std::string& body) const;
        bool isMultipartRequest(const Client& client) const;
        bool isRequestValid(const Client& client) const;
    public:
        POST();
        virtual ~POST();
        Response handleMultipartRequest(const Client& client, const std::string& target);
        Response handleRegularRequest(Client& client, const std::string& target);
        virtual Response execute(Client& client, const Server_block& server);
};

#endif
