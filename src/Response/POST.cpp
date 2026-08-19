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


bool POST::saveBody(const std::string& path, const std::string& body) const
{
    std::ofstream file(path.c_str(), std::ios::binary | std::ios::trunc);

    if (!file.is_open())
        return false;

    file.write(body.data(), static_cast<std::streamsize>(body.size()));

    bool success = file.good();
    file.close();
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

    path << "upload_" << static_cast<long>(time(NULL)) << "_" << nextUploadId();
    return path.str();
}

Response POST::handleRegularRequest(Client& client, const std::string& target)
{
    std::string destination = resolveDestination(target);
    PathType type = validateParentDirectory(destination);

    if (type == NOT_FOUND)
        return buildErrorResponse(404, "Not Found");

    if (type == PERMISSION_DENIED)
        return buildErrorResponse(403, "Forbidden");

    if (type != DIRECTORY_PATH)
        return buildErrorResponse(400, "Bad Request");

    if (!canWrite(destination))
        return buildErrorResponse(403, "Forbidden");

    const ClientRequest& request = client.parsed_request;

    if (request.usesTmpFile())
    {
        struct stat st;

        if (stat(request.getTmpFilePath().c_str(), &st) != 0)
            return buildErrorResponse(500, "Internal Server Error");
        off_t filesize = st.st_size;
        if (rename(request.getTmpFilePath().c_str(), destination.c_str()) != 0)
            return buildErrorResponse(500, "Internal Server Error");

        int fd = open(destination.c_str(), O_RDONLY);

        if (fd == -1)
            return buildErrorResponse(500, "Internal Server Error");

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

    return response;
}

bool POST::isRequestValid(const Client& client) const
{
    return !client.parsed_request.getRequestPath().empty();
}

Response POST::execute(Client& client, const Server_block& server)
{
    if (!isRequestValid(client))
        return buildErrorResponse(400, "Bad Request");

    const Location_Config* location = resolveLocation(client, server);
    std::string target = resolveTarget(client, server, location);

    if (target.empty())
        return buildErrorResponse(404, "Not Found");

    if (multiPart.isMultipartUpload(client.parsed_request.getHeaders()))
    {
        if (getPathType(target) != DIRECTORY_PATH)
            return buildErrorResponse(400, "Upload target must be a directory");
        if (!canWrite(target))
            return buildErrorResponse(403, "Forbidden");
        const ClientRequest& request = client.parsed_request;
        return multiPart.handleMultipartUpload(request.readBody(),request.getHeaders(),target);
    }
    return handleRegularRequest(client, target);
}
