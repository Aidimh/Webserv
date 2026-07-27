#include "Dispatcher.hpp"
#include "MethodFactory.hpp"

Response Dispatcher::dispatch(const HttpRequest& request,const Server_block& server,const Location_Config* location)
{
    AMethod* method = MethodFactory::createMethod(request.method);

    if (!method)
    {
        Response res;
        res.setStatusCode(501);
        res.setReasonPhrase("Not Implemented");
        return res;
    }

    Response response = method->execute(request, server, location);

    delete method;

    return response;
}