#include "MethodFactory.hpp"

AMethod* MethodFactory::createMethod(const std::string& method)
{
    if (method == "GET")
        return new GET();

    if (method == "POST")
        return new POST();

    if (method == "DELETE")
        return new DeleteMethod();

    return NULL;
}

// Response Server::handleRequest(HttpRequest& request)
// {
//     Server_block* server = findServer(request);

//     Location_Config* location =
//         findLocation(server, request.path);

//     if (!methodAllowed(location, request.method))
//         return 405;

//     AMethod* method =
//         MethodFactory::createMethod(request);

//     return method->execute(request, *server, location);
// }

// server_block    MethodFactory::Getserverblock()