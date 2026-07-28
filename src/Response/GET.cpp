#include "GET.hpp"
#include "../multiplexing/header.hpp"

GET::GET()
{
}

GET::~GET()
{
}


std::string GET::getContentType(const std::string& path) const
{
    size_t dotPos = path.find_last_of('.');

    if (dotPos == std::string::npos)
        return "application/octet-stream";

    std::string extension = path.substr(dotPos + 1);

    if (extension == "txt")
        return "text/plain";
    else if (extension == "html" || extension == "htm")
        return "text/html";
    else if (extension == "css")
        return "text/css";
    else if (extension == "js")
        return "application/javascript";
    else if (extension == "json")
        return "application/json";
    else if (extension == "png")
        return "image/png";
    else if (extension == "jpg" || extension == "jpeg")
        return "image/jpeg";
    else if (extension == "gif")
        return "image/gif";

    return "application/octet-stream";
}

std::string GET::readFile(const std::string& path, bool& success) const
{
    std::ifstream file(path.c_str(), std::ios::binary);

    if (!file.is_open())
    {
        success = false;
        return "";
    }

    std::stringstream buffer;
    buffer << file.rdbuf();

    success = true;
    return buffer.str();
}

Response GET::serveFile(const std::string& path) const
{
    bool success = false;
    std::string body = readFile(path, success);

    if (!success)
        return buildErrorResponse(500, "Internal Server Error");

    std::string type = getContentType(path);

    return buildFileResponse(body, type);
}

std::vector<std::string> GET::resolveIndexFiles(const Server_block& server, const Location_Config* location) const
{
    if (location != NULL && location->has_index && !location->index_files.empty())
        return location->index_files;

    return server.index_files;
}

bool GET::isAutoindexEnabled(const Server_block& server, const Location_Config* location) const
{
    if (location != NULL && location->has_autoindex)
        return location->autoindex == "on";

    return server.autoindex == "on";
}

Response GET::handleDirectory(const std::string& path, const Server_block& server, const Location_Config* location) const
{
    std::vector<std::string> indexFiles = resolveIndexFiles(server, location);

    for (size_t i = 0; i < indexFiles.size(); i++)
    {
        std::string candidatePath = path;

        if (!candidatePath.empty() && candidatePath[candidatePath.length() - 1] != '/')
            candidatePath += "/";

        candidatePath += indexFiles[i];

        if (fileExists(candidatePath) && !isDirectory(candidatePath))
            return serveFile(candidatePath);
    }

    if (isAutoindexEnabled(server, location))
    {
        std::string html = generateAutoIndex(path);

        if (html.empty())
            return buildErrorResponse(500, "Internal Server Error");

        return buildFileResponse(html, "text/html");
    }

    return buildErrorResponse(403, "Forbidden");
}

std::string GET::generateAutoIndex(const std::string& path) const
{
    DIR* dir = opendir(path.c_str());

    if (dir == NULL)
        return "";

    std::ostringstream html;

    buildAutoIndexHeader(html, path);

    handleDirectoryEntries(dir, path, html);

    buildAutoIndexFooter(html);

    closedir(dir);

    return html.str();
}

Response GET::buildFileResponse(const std::string& body, const std::string& contentType) const
{
    Response response;

    response.setStatusCode(200);
    response.setReasonPhrase("OK");
    response.setBody(body);               // Hadi automatically katdir Content-Length
    response.addHeader("content-type", contentType);

    return response;
}

Response GET::execute(Client& Client, const Server_block& server)
{
    std::string target = resolveTarget(request, server, location);

    if (target.empty())
        return buildErrorResponse(400, "Bad Request");
    switch (getPathType(target))
    {
        case NOT_FOUND:
            return buildErrorResponse(404, "Not Found");

        case FILE_PATH:
            return serveFile(target);

        case DIRECTORY_PATH:
            return handleDirectory(target, server, location);
        default:
            return buildErrorResponse(500, "Internal Server Error");
    }
}
