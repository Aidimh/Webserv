#include "../../includes/Response/Dispatcher.hpp"
#include "MethodFactory.hpp"
#include "../../includes/multiplexing/header.hpp"

Response Dispatcher::dispatch(Client& client,const Server_block& server)
{
    AMethod* method = MethodFactory::createMethod(client.parsed_request.getMethod());

    if (!method)
    {
        Response res;
        res.setStatusCode(501);
        res.setReasonPhrase("Not Implemented"); 
        return res;
    }

    Response response = method->execute(client, server);

    delete method;

    return response;
}