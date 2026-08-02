#include "Router.hpp"

bool Router::matchesLocation(const std::string& uri,const std::string& locationPath)
{
    if (locationPath.empty())
        return false;

    if (locationPath == "/")
        return !uri.empty() && uri[0] == '/';

    if (uri.compare(0, locationPath.size(), locationPath) != 0)
        return false;

    return uri.size() == locationPath.size()
        || uri[locationPath.size()] == '/';
}

const Location_Config* Router::resolveLocation(const std::string& uri,const Server_block& server)
{
    const Location_Config* match = NULL;
    size_t longestMatch = 0;

    for (size_t i = 0; i < server.location.size(); ++i)
    {
        const Location_Config& location = server.location[i];

        if (matchesLocation(uri, location.path)
            && location.path.size() > longestMatch)
        {
            match = &location;
            longestMatch = location.path.size();
        }
    }
    return match;
}

bool Router::isMethodAllowed(const std::string& method,const Location_Config& location)
{
    // Ila directive "allowed_methods" ma kaynach, nkhalli behavior lqdim:
    // l-handler howa li kay3alj method.
    if (location.allowed_methods.empty())
        return true;

    for (size_t i = 0; i < location.allowed_methods.size(); ++i)
    {
        if (location.allowed_methods[i] == method)
            return true;
    }
    return false;
}

bool Router::isCGIRequest(const ClientRequest& request,const Location_Config& location)
{
    size_t pos = request.getRequestPath().find('.');
    // PATH = www/folder/foledr/script.py
    //PATH = www/folder/script.c
    // PATH = .www/folder/script.js
    
    if (pos == std::string::npos)
        return false;

    std::string extension = request.getRequestPath().substr(pos);

    for (size_t i = 0; i < location.cgi_extensions.size(); ++i)
    {
        if (location.cgi_extensions[i] == extension)
            return true;
    }
    return false;
}