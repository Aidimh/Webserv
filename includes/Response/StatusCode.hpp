#ifndef STATUSCODE_HPP
#define STATUSCODE_HPP

enum HttpStatusCode
{
    // Success
    OK                  = 200,
    CREATED             = 201,
    NO_CONTENT          = 204,

    // Client Errors
    BAD_REQUEST         = 400,
    UNAUTHORIZED        = 401,
    FORBIDDEN           = 403,
    NOT_FOUND           = 404,
    METHOD_NOT_ALLOWED  = 405,
    REQUEST_TIMEOUT     = 408,
    PAYLOAD_TOO_LARGE   = 413,

    // Server Errors
    INTERNAL_SERVER_ERROR = 500,
    NOT_IMPLEMENTED       = 501,
    BAD_GATEWAY           = 502,
    SERVICE_UNAVAILABLE   = 503,
    GATEWAY_TIMEOUT       = 504
};

#endif