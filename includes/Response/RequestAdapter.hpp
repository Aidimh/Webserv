#ifndef REQUESTADAPTER_HPP
#define REQUESTADAPTER_HPP

#include "ClientRequest.hpp"
#include "HttpRequest.hpp"

// class Server_block;

class RequestAdapter
{
    public:
        // Server_block* server_block;
        // RequestAdapter(Server_block* server_block) : server_block(server_block) {}
        static HttpRequest toHttpRequest(const ClientRequest& client);
};

#endif