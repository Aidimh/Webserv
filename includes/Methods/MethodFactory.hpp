#ifndef METHODFACTORY_HPP
#define METHODFACTORY_HPP

#include "AMethod.hpp"
#include "GET.hpp"
#include "POST.hpp"
#include "DeleteMethod.hpp"

class MethodFactory
{
    public:
        static AMethod* createMethod(const std::string& method);
};

#endif
