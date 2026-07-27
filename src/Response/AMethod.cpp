#include "AMethod.hpp"
#include "../multiplexing/header.hpp"

AMethod::~AMethod()
{
}

Response AMethod::buildErrorResponse(int statusCode, const std::string& message) const
{
    Response response;

    response.setStatusCode(statusCode);
    response.setReasonPhrase(message);

    std::ostringstream html;

    html << "<html>"
         << "<body>"
         << "<h1>"
         << statusCode
         << " "
         << message
         << "</h1>"
         << "</body>"
         << "</html>";

    std::string body = html.str();

    response.setBody(body);
    response.addHeader("content-type", "text/html");

    std::ostringstream oss;
    oss << body.size();

    response.addHeader("Content-Length", oss.str());

    return response;
}


std::string AMethod::resolveTarget(const HttpRequest& request,const Server_block& server,const Location_Config* location) const
{
    std::string root = server.root;

    if (location && !location->root.empty())
        root = location->root;

    return root + request.path;
}

//POST /salah.mp4
//Path =  /home/moel-aid/Desktop/Webserv/multiplexing/salah

PathType AMethod::getPathType(const std::string& path) const
{
    
    if (fileExists(path))
    {
        if (isDirectory(path))
            return DIRECTORY_PATH;
        else
            return FILE_PATH;
    }
    else
        return NOT_FOUND;
}

bool AMethod::fileExists(const std::string& path) const
{
    struct stat info;

    return (stat(path.c_str(), &info) == 0);
}

bool AMethod::isDirectory(const std::string& path) const
{
    struct stat info;

    if (stat(path.c_str(), &info) != 0)
        return false;

    return S_ISDIR(info.st_mode);
}

