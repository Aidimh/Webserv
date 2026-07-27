#ifndef GET_HPP
#define GET_HPP

#include "AMethod.hpp"
#include "Response.hpp"
#include "PathType.hpp"
#include "HttpRequest.hpp"


#include <dirent.h> // open directory
#include <fstream>
#include <sstream>
#include <string>
#include <iostream>
#include <stdexcept>
#include <sys/stat.h>
#include <vector>


class GET : public AMethod
{
    private:
        std::string readFile(const std::string& path, bool& success) const;
        std::string getContentType(const std::string& path) const;
        Response serveFile(const std::string& path) const;
        Response handleDirectory(const std::string& path,const Server_block& server,const Location_Config* location) const;
        std::string generateAutoIndex(const std::string& path) const;
        void    buildAutoIndexHeader(std::ostringstream& html,const std::string& path) const;
        void    buildAutoIndexFooter(std::ostringstream& html) const;
        void    handleDirectoryEntries(DIR* dir,const std::string& currentPath,std::ostringstream& html) const;
        bool    isHiddenEntry(const std::string& name) const;
        bool    isDirectoryEntry(const std::string& currentPath,const std::string& name) const;
        void    appendDirectoryLink(std::ostringstream& html,const std::string& name) const;
        void    appendFileLink(std::ostringstream& html,const std::string& name) const;
        Response buildFileResponse(const std::string& body,const std::string& contentType) const;
        std::vector<std::string> resolveIndexFiles(const Server_block& server, const Location_Config* location) const;
        bool isAutoindexEnabled(const Server_block& server, const Location_Config* location) const;


    public:
        GET();
        virtual ~GET();

        virtual Response execute(const HttpRequest& request,const Server_block& server,const Location_Config* location);
        // virtual Response execute(const HttpRequest& request);
};


#endif