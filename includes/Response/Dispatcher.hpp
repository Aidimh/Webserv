#ifndef DISPATCHER_HPP
#define DISPATCHER_HPP

#include "MethodFactory.hpp"
#include "../multiplexing/header.hpp"
#include "../Request/ClientRequest.hpp"

class Dispatcher
{
    public:
        static Response dispatch(Client& client, const Server_block& server);
        static void setErrorPageBody(Response& response, const Server_block& server);
};

#endif