#include "../../includes/Response/Dispatcher.hpp"
#include "MethodFactory.hpp"
#include "../../includes/multiplexing/header.hpp"

static std::string statusMessage(short code)
{
    switch (code)
    {
        case HTTP_400_BAD_REQUEST:
            return "Bad Request";

        case HTTP_403_FORBIDDEN:
            return "Forbidden";

        case HTTP_404_NOT_FOUND:
            return "Not Found";

        case HTTP_405_METHOD_NOT_ALLOWED:
            return "Method Not Allowed";

        case HTTP_408_REQUEST_TIMEOUT:
            return "Request Timeout";

        case HTTP_409_CONFLICT:
            return "Conflict";

        case HTTP_410_GONE:
            return "Gone";

        case HTTP_411_LENGTH_REQUIRED:
            return "Length Required";

        case HTTP_413_PAYLOAD_TOO_LARGE:
            return "Payload Too Large";

        case HTTP_414_URI_TOO_LONG:
            return "URI Too Long";

        case HTTP_415_UNSUPPORTED_MEDIA:
            return "Unsupported Media Type";

        case HTTP_500_INTERNAL_SERVER_ERROR:
            return "Internal Server Error";

        case HTTP_501_NOT_IMPLEMENTED:
            return "Not Implemented";

        case HTTP_502_BAD_GATEWAY:
            return "Bad Gateway";

        case HTTP_504_GATEWAY_TIMEOUT:
            return "Gateway Timeout";

        case HTTP_505_HTTP_VERSION_NOT_SUPPORTED:
            return "HTTP Version Not Supported";

        default:
            return "Unknown Error";
    }
}
Response Dispatcher::dispatch(Client& client,const Server_block& server)
{
    // 1. Parser already found an error
    if (client.parsed_request.getStatusCode() != 200)
        return AMethod::buildErrorResponse(client.parsed_request.getStatusCode(),statusMessage(client.parsed_request.getStatusCode()));
    //. Normal request
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