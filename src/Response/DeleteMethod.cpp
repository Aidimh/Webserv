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
    return response;
}

Response DeleteMethod::execute(Client& client, const Server_block& server)
{
    if (client.parsed_request.getRequestPath().empty())
        return buildErrorResponse(400, "Bad Request");
    const Location_Config* location = resolveLocation(client, server);
    std::string target = resolveTarget(client, server, location);
    // std::cout << "[DEBUG DELETE TARGET]: " << target << std::endl;
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
