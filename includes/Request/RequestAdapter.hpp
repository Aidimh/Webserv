#ifndef REQUESTADAPTER_HPP
#define REQUESTADAPTER_HPP

#include "ClientRequest.hpp"
#include "HttpRequest.hpp"

class RequestAdapter
{
    public:
        static HttpRequest toHttpRequest(const ClientRequest& client);
};

#endif