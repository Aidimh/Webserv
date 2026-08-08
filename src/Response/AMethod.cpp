#include "AMethod.hpp"
#include "../multiplexing/header.hpp"

AMethod::~AMethod()
{
}

std::string AMethod::urlDecode(const std::string& url) const
{
    std::string result;

    for (size_t i = 0; i < url.length(); ++i)
    {
        if (url[i] == '%' && i + 2 < url.length())
        {
            char hex[3];
            hex[0] = url[i + 1];
            hex[1] = url[i + 2];
            hex[2] = '\0';

            char decoded = static_cast<char>(strtol(hex, NULL, 16));

            result += decoded;
            i += 2;
        }
        else if (url[i] == '+')
        {
            result += ' ';
        }
        else
        {
            result += url[i];
        }
    }

    return result;
}

Response AMethod::buildErrorResponse(short statusCode, const std::string& message)
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
    return response;
}

const Location_Config* AMethod::resolveLocation(const Client& client, const Server_block& server) const
{
    return Router::resolveLocation(client.parsed_request.getRequestPath(), server);
}

std::string AMethod::normalizePath(const std::string& path, bool& outOfBounds) const
{
    std::vector<std::string> stack;
    outOfBounds = false;

    size_t i = 0;

    while (i < path.size())
    {
        while (i < path.size() && path[i] == '/')
            ++i;

        if (i >= path.size())
            break;

        size_t start = i;

        while (i < path.size() && path[i] != '/')
            ++i;

        std::string segment = path.substr(start, i - start);

        if (segment == "." || segment.empty())
            continue;

        if (segment == "..")
        {
            if (stack.empty())
            {
                outOfBounds = true;
                return "";
            }
            stack.pop_back();
        }
        else
        {
            stack.push_back(segment);
        }
    }

    std::string result;

    for (size_t j = 0; j < stack.size(); ++j)
        result += "/" + stack[j];

    if (result.empty())
        result = "/";

    return result;
}

std::string AMethod::resolveTarget(const Client& client,const Server_block& server,const Location_Config* location) const
{
    std::cout << "===== GET DEBUG =====\n";
    std::cout << "Request path: [" << client.parsed_request.getRequestPath() << "]\n";
    if (location)
    {
        std::cout << "Location path: [" << location->path << "]\n";
        std::cout << "Location root: [" << location->root << "]\n";
    }
    std::cout << "Server root: [" << server.root << "]\n";
    std::string requestPath = urlDecode(client.parsed_request.getRequestPath());
    // const std::string& requestPath = client.parsed_request.getRequestPath();

    // path = "/"

    std::string root = server.root;
    std::string locationPath;

    if (location)
    {
        if (!location->root.empty())
            root = location->root;
        locationPath = location->path;
    }

    // Defensive programming
    if (requestPath.size() < locationPath.size())
        return "";

    // Remove trailing '/' from root (except "/")
    if (root.size() > 1 && root[root.size() - 1] == '/')
        root.erase(root.size() - 1);

    std::string relativePath = requestPath.substr(locationPath.size());

    bool outOfBounds = false;
    std::string normalizedRelative = normalizePath("/" + relativePath, outOfBounds);

    if (outOfBounds)
        return "";

    return root + normalizedRelative;
}


PathType AMethod::getPathType(const std::string& path) const
{
    if (fileExists(path))
    {
        if(access(path.c_str(), R_OK) != 0)
            return PERMISSION_DENIED;
        else if (isDirectory(path))
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

