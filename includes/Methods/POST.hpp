#ifndef POST_HPP
#define POST_HPP

#include "AMethod.hpp"
#include "Response.hpp"
#include "PathType.hpp"
#include "MultipartUploadStrategy.hpp"

#include <sys/stat.h>
#include <fstream>
#include <unistd.h>
#include <ctime>
#include <sstream>

class POST : public AMethod
{
    private:
        MultipartUploadStrategy multiPart;

        std::string getParentDirectory(const std::string& target) const;
        bool canWrite(const std::string& path) const;
        PathType validateParentDirectory(const std::string& target) const;
        bool saveBody(const std::string& path, const std::string& body) const;
        bool isRequestValid(const Client& client) const;
        std::string resolveDestination(const std::string& target) const;

    public:
        POST();
        virtual ~POST();
        Response handleRegularRequest(Client& client,const std::string& target);
        virtual Response execute(Client& client,const Server_block& server);
};

#endif
