#include "DeleteMethod.hpp"

DeleteMethod::DeleteMethod()
{
}

DeleteMethod::~DeleteMethod()
{
}

bool DeleteMethod::canDelete(const std::string& path) const
{
    // Check if the file exists and is writable
    return access(path.c_str(), W_OK) == 0;
}

Response DeleteMethod::buildNoContentResponse() const
{
    Response response;
    
    response.setStatusCode(204);
    response.setReasonPhrase("No Content");
    response.setBody("");
    response.addHeader("Content-Length","0");
    return response;
}

Response DeleteMethod::execute(const HttpRequest& request,const Server_block& server,const Location_Config* location)
{
    if (request.path.empty())
        return buildErrorResponse(400, "Bad Request");

    std::string target = resolveTarget(request, server, location);

    if (!fileExists(target))
        return buildErrorResponse(404, "Not Found");

    if (isDirectory(target))
        return buildErrorResponse(403, "Forbidden");

    if (!canDelete(target))
        return buildErrorResponse(403, "Forbidden");

    if (remove(target.c_str()) != 0)
        return buildErrorResponse(500, "Internal Server Error");

    return buildNoContentResponse();
}
