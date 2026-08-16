#include "GET.hpp"
#include "../multiplexing/header.hpp"
#include "../Logging/Logging.hpp"

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
        ERR() << "GET::readFile: open file failed path=" << path << ": " << strerror(errno);
        success = false;
        return "";
    }

    std::stringstream buffer;
    buffer << file.rdbuf();

    success = true;
    DEBUG("GET") << "readFile: read " << buffer.str().size() << " bytes from path=" << path;
    return buffer.str();
}

Response GET::serveFile(Client& client, const std::string& path) const
{
    struct stat fileInfo;

    if (stat(path.c_str(), &fileInfo) != 0)
    {
        ERR() << "GET::serveFile: stat failed path=" << path << ": " << strerror(errno);
        return buildErrorResponse(500, "Internal Server Error");
    }

    std::string type = getContentType(path);

    if (fileInfo.st_size > STREAMING_THRESHOLD)
    {
        int streamFd = open(path.c_str(), O_RDONLY);

        if (streamFd == -1)
        {
            ERR() << "GET::serveFile: open file failed path=" << path << ": " << strerror(errno);
            return buildErrorResponse(500, "Internal Server Error");
        }

        DEBUG("GET") << "serveFile: opened stream file fd=" << streamFd << " path=" << path
                     << " size=" << fileInfo.st_size << " bytes, streaming";
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

    DEBUG("GET") << "serveFile: serving path=" << path << " size=" << body.size()
                 << " bytes content_type=" << type;
    return buildFileResponse(body, type);
}

std::vector<std::string> GET::resolveIndexFiles(const Server_block& server, const Location_Config* location) const
{
    if (location != NULL && location->has_index && !location->index_files.empty())
        return location->index_files;

    return server.index_files;
}


bool GET::isAutoindexEnabled(const Server_block& server,const Location_Config* location) const
{
    // location override
    if (location != NULL && location->has_autoindex)
        return location->autoindex == "on";

    // fallback to server-level
    if (server.server_has_autoindex)
        return server.server_auto_index == "on";

    // default
    return false;
}


Response GET::handleDirectory(Client& client, const std::string& path, const Server_block& server, const Location_Config* location) const
{
    std::vector<std::string> indexFiles = resolveIndexFiles(server, location);

    for (size_t i = 0; i < indexFiles.size(); i++)
    {
        std::string candidatePath = path;
        // std::cout<<"path = "<<path<<std::endl;
        if (!candidatePath.empty() && candidatePath[candidatePath.length() - 1] != '/')
            candidatePath += "/";

        candidatePath += indexFiles[i];
        if (fileExists(candidatePath) && !isDirectory(candidatePath))
        {
            DEBUG("GET") << "handleDirectory: serving index file path=" << candidatePath;
            return serveFile(client, candidatePath);
        }
    }
    if (isAutoindexEnabled(server, location))
    {
        DEBUG("GET") << "handleDirectory: no index file found, generating autoindex for path=" << path;
        std::string html = generateAutoIndex(path);

        if (html.empty())
            return buildErrorResponse(500, "Internal Server Error");

        return buildFileResponse(html, "text/html");
    }

    DEBUG("GET") << "handleDirectory: no index file and autoindex off, responding status=403 path=" << path;
    return buildErrorResponse(403, "Forbidden");
}

std::string GET::generateAutoIndex(const std::string& path) const
{
    DIR* dir = opendir(path.c_str());

    if (dir == NULL)
    {
        ERR() << "GET::generateAutoIndex: opendir failed path=" << path << ": " << strerror(errno);
        return "";
    }
    DEBUG("GET") << "generateAutoIndex: opened directory path=" << path;

    std::ostringstream html;

    buildAutoIndexHeader(html, path);

    handleDirectoryEntries(dir, path, html);

    buildAutoIndexFooter(html);

    closedir(dir);
    DEBUG("GET") << "generateAutoIndex: closed directory path=" << path;

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

Response    GET::buildRedirectResponse(const std::string& requestPath) const
{
    Response response;

    response.setStatusCode(301);
    response.setReasonPhrase("Moved Permanently");
    response.addHeader("Location", requestPath + "/");
    response.setBody("");

    return response;
}

bool    GET::needsDirectoryRedirect(const std::string& requestPath,const std::string& target) const
{
    if (!isDirectory(target))
        return false;

    if (requestPath.empty())
        return false;

    return requestPath[requestPath.size() - 1] != '/';
}


Response GET::execute(Client& client, const Server_block& server)
{
    const Location_Config* location = resolveLocation(client, server);
    std::string target = resolveTarget(client, server, location);
    const std::string& requestPath = client.parsed_request.getRequestPath();

    DEBUG("GET") << "execute: uri=" << requestPath << " target=" << target << " fd=" << client.fd;

    if (target.empty())
    {
        DEBUG("GET") << "execute: target resolution failed, responding status=403 uri=" << requestPath;
        return buildErrorResponse(403, "Forbidden");
    }
    PathType type = getPathType(target);


    // std::cout << "===== PATH TYPE DEBUG =====\n";
    // std::cout << "target = [" << target << "]\n";
    // if (type == NOT_FOUND)
    //     std::cout << "type = NOT_FOUND\n";
    // else if (type == DIRECTORY_PATH)
    //     std::cout << "type = DIRECTORY_PATH\n";
    // else if (type == FILE_PATH)
    //     std::cout << "type = FILE_PATH\n";
    // else if (type == PERMISSION_DENIED)
    //     std::cout << "type = PERMISSION_DENIED\n";

    
    // std::cout << "===========================\n";

    switch (type)
    {
        case PERMISSION_DENIED:
            DEBUG("GET") << "execute: target not readable, responding status=403 target=" << target;
            return buildErrorResponse(403, "Forbidden");
        case NOT_FOUND:
            DEBUG("GET") << "execute: target does not exist, responding status=404 target=" << target;
            return buildErrorResponse(404, "Not Found");
        case FILE_PATH:
            return serveFile(client, target);
        case DIRECTORY_PATH:
            if (needsDirectoryRedirect(requestPath, target))
            {
                DEBUG("GET") << "execute: directory without trailing slash, responding status=301 uri=" << requestPath;
                return buildRedirectResponse(requestPath);
            }
            return handleDirectory(client, target, server, location);
        default:
            ERR() << "GET::execute: unknown path type for target=" << target;
            return buildErrorResponse(500, "Internal Server Error");
    }
}

