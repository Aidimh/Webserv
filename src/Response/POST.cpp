#include "POST.hpp"
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

Response POST::buildCreatedResponse(int statusCode, const std::string& reasonPhrase) const
{
    Response response;

    response.setStatusCode(statusCode);
    response.setReasonPhrase(reasonPhrase);

    response.setBody("");
    response.addHeader("content-type", "text/plain");

    return response;
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

Response POST::handleRegularRequest(const Client& client, const std::string& target)
{
    PathType type = validateParentDirectory(target);

    if (type == NOT_FOUND)
        return buildErrorResponse(404, "Not Found");

    if (type == PERMISSION_DENIED)
        return buildErrorResponse(403, "Forbidden");

    if (type != DIRECTORY_PATH)
        return buildErrorResponse(400, "Bad Request");

    if (!canWrite(target))
        return buildErrorResponse(403, "Forbidden");

    if (fileExists(target))
        return buildErrorResponse(409, "Conflict");

    if (!saveBody(target, client.parsed_request.getBody()))
        return buildErrorResponse(500, "Internal Server Error");

    return buildCreatedResponse(201, "Created");
}


Response POST::handleMultipartRequest(const Client& client, const std::string& target)
{
    if (getPathType(target) != DIRECTORY_PATH)
        return buildErrorResponse(400, "Upload target must be a directory");

    if (!canWrite(target))
        return buildErrorResponse(403, "Forbidden");

    return multiPart.handleMultipartUpload(client.parsed_request, target);
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
        return buildErrorResponse(400, "Bad Request");

    const Location_Config* location = resolveLocation(client, server);
    std::string target = resolveTarget(client, server, location);

    if (target.empty())
        return buildErrorResponse(403, "Forbidden");
    if (isMultipartRequest(client))
        return handleMultipartRequest(client, target);

    return handleRegularRequest(client, target);
}
