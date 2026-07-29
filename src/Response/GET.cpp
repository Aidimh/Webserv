#include "GET.hpp"
#include "../multiplexing/header.hpp"

#include <fcntl.h>
#include <unistd.h>

const off_t STREAMING_THRESHOLD = 8 * 1024 * 1024;

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

Response GET::serveFile(Client& client, const std::string& path) const
{
    struct stat fileInfo;

    if (stat(path.c_str(), &fileInfo) != 0)
        return buildErrorResponse(500, "Internal Server Error");

    std::string type = getContentType(path);

    if (fileInfo.st_size > STREAMING_THRESHOLD)
    {
        int streamFd = open(path.c_str(), O_RDONLY);

        if (streamFd == -1)
            return buildErrorResponse(500, "Internal Server Error");

        client.stream_file_fd = streamFd;
        client.stream_bytes_remaining = fileInfo.st_size;
        Response response = buildStreamingFileResponse(fileInfo.st_size, type);
        response.setResponseMode(Response::STREAMING_RESPONSE);
        return response;
    }

    bool success = false;
    std::string body = readFile(path, success);

    if (!success)
        return buildErrorResponse(500, "Internal Server Error");

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

Response GET::handleDirectory(Client& client, const std::string& path, const Server_block& server, const Location_Config* location) const
{
    std::vector<std::string> indexFiles = resolveIndexFiles(server, location);

    for (size_t i = 0; i < indexFiles.size(); i++)
    {
        std::string candidatePath = path;

        if (!candidatePath.empty() && candidatePath[candidatePath.length() - 1] != '/')
            candidatePath += "/";

        candidatePath += indexFiles[i];

        if (fileExists(candidatePath) && !isDirectory(candidatePath))
            return serveFile(client, candidatePath);
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

Response GET::buildStreamingFileResponse(off_t fileSize, const std::string& contentType) const
{
    Response response;
    std::ostringstream length;

    response.setStatusCode(200);
    response.setReasonPhrase("OK");
    length << fileSize;
    response.addHeader("Content-Length", length.str());
    response.addHeader("content-type", contentType);

    return response;
}

Response GET::execute(Client& client, const Server_block& server)
{
    const Location_Config* location = resolveLocation(client, server);
    std::string target = resolveTarget(client, server, location);

    if (target.empty())
        return buildErrorResponse(400, "Bad Request");
    switch (getPathType(target))
    {
        case NOT_FOUND:
            return buildErrorResponse(404, "Not Found");

        case FILE_PATH:
            return serveFile(client, target);

        case DIRECTORY_PATH:
            return handleDirectory(client, target, server, location);
        default:
            return buildErrorResponse(500, "Internal Server Error");
    }
}
