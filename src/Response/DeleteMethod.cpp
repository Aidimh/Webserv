#include "DeleteMethod.hpp"
#include "../Logging/Logging.hpp"

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
    {
        DEBUG("DeleteMethod") << "execute: empty request path, responding status=400 fd=" << client.fd;
        return buildErrorResponse(400, "Bad Request");
    }

    const Location_Config* location = resolveLocation(client, server);
    std::string target = resolveTarget(client, server, location);

    DEBUG("DeleteMethod") << "execute: uri=" << client.parsed_request.getRequestPath()
                          << " target=" << target << " fd=" << client.fd;

    if (!fileExists(target))
    {
        DEBUG("DeleteMethod") << "execute: target does not exist, responding status=404 target=" << target;
        return buildErrorResponse(404, "Not Found");
    }
    if (isDirectory(target))
    {
        DEBUG("DeleteMethod") << "execute: target is a directory, responding status=403 target=" << target;
        return buildErrorResponse(403, "Forbidden");
    }
    if (!canDelete(target))
    {
        DEBUG("DeleteMethod") << "execute: target is not writable, responding status=403 target=" << target;
        return buildErrorResponse(403, "Forbidden");
    }
    if (remove(target.c_str()) != 0)
    {
        ERR() << "DeleteMethod::execute: remove failed target=" << target << ": " << strerror(errno);
        return buildErrorResponse(500, "Internal Server Error");
    }
    INFO() << "DeleteMethod::execute: deleted target=" << target;
    return buildNoContentResponse();
}
