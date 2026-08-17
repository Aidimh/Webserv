#include "POST.hpp"
#include "../Logging/Logging.hpp"
#include <unistd.h>

POST::POST()
{
}

POST::~POST()
{
}

std::string POST::getParentDirectory(const std::string& target) const
{
    size_t pos = target.find_last_of('/');

    if (pos == std::string::npos)
        return ".";

    return target.substr(0, pos);
}

bool POST::canWrite(const std::string& target) const
{
    std::string parent = getParentDirectory(target);
    return (access(parent.c_str(), W_OK) == 0);
}

// Response POST::buildCreatedResponse(int statusCode, const std::string& reasonPhrase) const
// {
//     Response response;

//     response.setStatusCode(statusCode);
//     response.setReasonPhrase(reasonPhrase);

//     response.setBody("");
//     response.addHeader("content-type", "text/plain");

//     return response;
// }

bool POST::saveBody(const std::string& path, const std::string& body) const
{
    std::ofstream file(path.c_str(), std::ios::binary | std::ios::trunc);

    if (!file.is_open())
    {
        ERR() << "POST::saveBody: open file failed path=" << path << ": " << strerror(errno);
        return false;
    }

    file.write(body.data(), static_cast<std::streamsize>(body.size()));

    bool success = file.good();
    file.close();

    if (success)
        DEBUG("POST") << "saveBody: wrote " << body.size() << " bytes to path=" << path;
    else
        ERR() << "POST::saveBody: write failed path=" << path;

    return success;
}

PathType POST::validateParentDirectory(const std::string& target) const
{
    std::string parent = getParentDirectory(target);

    return getPathType(parent);
}


static unsigned long nextUploadId()
{
    static unsigned long counter = 0;
    return ++counter;
}

std::string POST::resolveDestination(const std::string& target) const
{
    if (getPathType(target) != DIRECTORY_PATH)
        return target;

    std::ostringstream path;

    path << target;

    if (target.empty() || target[target.size() - 1] != '/')
        path << "/";

    //The counter helps avoid having the exact same name if multiple uploads happen during the same second.
    path << "upload_" << static_cast<long>(time(NULL)) << "_" << nextUploadId();
    return path.str();
}

Response POST::handleRegularRequest(Client& client, const std::string& target)
{
    std::string destination = resolveDestination(target);
    PathType type = validateParentDirectory(destination);

    if (type == NOT_FOUND)
    {
        DEBUG("POST") << "handleRegularRequest: parent directory missing, responding status=404 target=" << destination;
        return buildErrorResponse(404, "Not Found");
    }

    if (type == PERMISSION_DENIED)
    {
        DEBUG("POST") << "handleRegularRequest: parent directory not readable, responding status=403 target="  << destination;
        return buildErrorResponse(403, "Forbidden");
    }

    if (type != DIRECTORY_PATH)
    {
        DEBUG("POST") << "handleRegularRequest: parent is not a directory, responding status=400 target=" << destination;
        return buildErrorResponse(400, "Bad Request");
    }

    if (!canWrite(destination))
    {
        DEBUG("POST") << "handleRegularRequest: parent directory not writable, responding status=403 target=" << destination;
        return buildErrorResponse(403, "Forbidden");
    }

    const ClientRequest& request = client.parsed_request;

    if (request.usesTmpFile())
    {
        struct stat st;

        if (stat(request.getTmpFilePath().c_str(), &st) != 0)
        {
            ERR() << "POST::handleRegularRequest: stat failed on temp file path="
                  << request.getTmpFilePath() << ": " << strerror(errno);
            return buildErrorResponse(500, "Internal Server Error");
        }

        off_t filesize = st.st_size;

        if (rename(request.getTmpFilePath().c_str(), destination.c_str()) != 0)
        {
            ERR() << "POST::handleRegularRequest: rename failed from="
                  << request.getTmpFilePath()
                  << " to=" << destination
                  << ": " << strerror(errno);
            return buildErrorResponse(500, "Internal Server Error");
        }

        int fd = open(destination.c_str(), O_RDONLY);

        if (fd == -1)
        {
            ERR() << "POST::handleRegularRequest: open target failed path="
                  << destination << ": " << strerror(errno);
            return buildErrorResponse(500, "Internal Server Error");
        }

        client.stream_file_fd = fd;
        client.stream_bytes_remaining = filesize;

        Response response;
        response.setStatusCode(201);
        response.setReasonPhrase("Created");

        std::ostringstream oss;
        oss << filesize;

        response.addHeader("Content-Length", oss.str());
        response.addHeader("content-type", "text/plain");
        response.setResponseMode(Response::STREAMING_RESPONSE);

        return response;
    }

    if (!saveBody(destination, request.getBody()))
        return buildErrorResponse(500, "Internal Server Error");

    Response response;
    response.setStatusCode(201);
    response.setReasonPhrase("Created");
    response.setBody(request.getBody());
    response.addHeader("content-type", "text/plain");

    INFO() << "POST::handleRegularRequest: created target=" << destination << " size=" << request.getBody().size() << " bytes";

    return response;
}

// Response POST::handleRegularRequest(const Client& client, const std::string& target)
// {
//     PathType type = validateParentDirectory(target);

//     if (type == NOT_FOUND)
//         return buildErrorResponse(404, "Not Found");

//     if (type == PERMISSION_DENIED)
//         return buildErrorResponse(403, "Forbidden");

//     if (type != DIRECTORY_PATH)
//         return buildErrorResponse(400, "Bad Request");

//     if (!canWrite(target))
//         return buildErrorResponse(403, "Forbidden");

//     const ClientRequest& request = client.parsed_request;

//     // std::cout<<"Test\n";
//     // Case 1: body kbir, streamed l temp file → ghir bddel smiytou l target
//     if (request.usesTmpFile())
//     {
//         std::cout<<"Test\n";
//         if (rename(request.getTmpFilePath().c_str(), target.c_str()) != 0)
//             return buildErrorResponse(500, "Internal Server Error");
//         return buildCreatedResponse(201, "Created");
//     }

//     // Case 2: body sghir, f RAM → save 3adi
//     if (!saveBody(target, request.getBody()))
//         return buildErrorResponse(500, "Internal Server Error");

//     return buildCreatedResponse(201, "Created");
// }


Response POST::handleMultipartRequest(const Client& client, const std::string& target)
{
if (getPathType(target) != DIRECTORY_PATH)
    {
        DEBUG("POST") << "handleMultipartRequest: upload target is not a directory, responding status=400 target="
                      << target;
        return buildErrorResponse(400, "Upload target must be a directory");
    }

    if (!canWrite(target))
    {
        DEBUG("POST") << "handleMultipartRequest: upload target not writable, responding status=403 target="
                      << target;
        return buildErrorResponse(403, "Forbidden");
    }

    DEBUG("POST") << "handleMultipartRequest: handling multipart upload into target=" << target;
    std::string body = client.parsed_request.readBody();
    return multiPart.handleMultipartUpload(body, client.parsed_request.getHeaders(), target);
}

bool POST::isMultipartRequest(const Client& client) const
{
    const std::map<std::string, std::string>& headers = client.parsed_request.getHeaders();
    std::map<std::string, std::string>::const_iterator it = headers.find("content-type");

    if (it == headers.end())
        return false;

    const std::string& contentType = it->second;

    return contentType.find("multipart/") != std::string::npos;
}

bool POST::isRequestValid(const Client& client) const
{
    return !client.parsed_request.getRequestPath().empty();
}


Response POST::execute(Client& client, const Server_block& server)
{
    if (!isRequestValid(client))
    {
        DEBUG("POST") << "execute: empty request path, responding status=400 fd=" << client.fd;
        return buildErrorResponse(400, "Bad Request");
    }
    const Location_Config* location = resolveLocation(client, server);
    std::string target = resolveTarget(client, server, location);

    DEBUG("POST") << "execute: uri=" << client.parsed_request.getRequestPath() << " target=" << target << " fd=" << client.fd;
    if (target.empty())
    {
        DEBUG("POST") << "execute: target resolution failed, responding status=403 fd=" << client.fd;
        return buildErrorResponse(404, "Not Found");
    }

    const ClientRequest& request = client.parsed_request;
    std::string body = request.ClientRequest::readBody();
    if (isMultipartRequest(client))
    {
        DEBUG("POST") << "execute: multipart request, body_size=" << body.size() << " bytes fd=" << client.fd;
        return multiPart.handleMultipartUpload(body,request.getHeaders(),target);
    }
    return handleRegularRequest(client, target);
}

resolveDestination