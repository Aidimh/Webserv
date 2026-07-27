#ifndef METHODFACTORY_HPP
#define METHODFACTORY_HPP

#include "AMethod.hpp"
#include "GET.hpp"
#include "POST.hpp"
#include "DeleteMethod.hpp"
#include <string>

class MethodFactory
{
    public:
        // Server_block* Getserverblock();
        static AMethod* createMethod(const std::string& method);
};

#endif
