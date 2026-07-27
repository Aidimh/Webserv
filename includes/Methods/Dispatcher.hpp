#ifndef DISPATCHER_HPP
#define DISPATCHER_HPP

#include "MethodFactory.hpp"

class Dispatcher
{
    public:
        static Response dispatch(const HttpRequest& request,const Server_block& server,const Location_Config* location);
};

#endif