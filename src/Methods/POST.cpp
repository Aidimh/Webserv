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
    response.addHeader("Content-Length", "0");
    response.addHeader("content-type", "text/plain");

    return response;
}

bool POST::saveBody(const std::string& path, const std::string& body) const
{
    std::ofstream file(path.c_str(), std::ios::binary | std::ios::trunc);

    if (!file.is_open())
        return false;

    file.write(body.data(),
           static_cast<std::streamsize>(body.size()));
    bool success = file.good();

    file.close();
    return success;
}

bool POST::validateParentDirectory(const std::string& target) const
{
    std::string parent = getParentDirectory(target);

    return (getPathType(parent) == DIRECTORY_PATH);
}

Response POST::handleRegularRequest(const HttpRequest& request,const std::string& target)
{
    if (!validateParentDirectory(target))
        return buildErrorResponse(404, "Not Found");
    if (!canWrite(target))
        return buildErrorResponse(403, "Forbidden");

    if (!saveBody(target, request.body))
        return buildErrorResponse(500, "Internal Server Error");

    return buildCreatedResponse(200, "Ok");
}

Response POST::handleMultipartRequest(const HttpRequest& request,const std::string& target)
{
    if (getPathType(target) != DIRECTORY_PATH)
        return buildErrorResponse(400, "Upload target must be a directory");

    if (!canWrite(target))
        return buildErrorResponse(403, "Forbidden");

    return multiPart.handleMultipartUpload(request, target);
}

bool POST::isMultipartRequest(const HttpRequest& request) const
{
    if (request.headers.find("content-type") == request.headers.end())
        return false;

    std::string contentType = request.headers.at("content-type");

    return contentType.find("multipart/") != std::string::npos;
}

bool POST::isRequestValid(const HttpRequest& request) const
{
    return !request.path.empty();
}

Response POST::execute(const HttpRequest& request,const Server_block& server,const Location_Config* location)
{
    if (!isRequestValid(request))
        return buildErrorResponse(400, "Bad Request");

    std::string target = resolveTarget(request, server, location);

    if (isMultipartRequest(request))
        return handleMultipartRequest(request, target);

    // std::cout<<"Target : "<< target<<std::endl;
    return handleRegularRequest(request, target);
}
