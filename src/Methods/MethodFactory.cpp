#include "MethodFactory.hpp"

AMethod* MethodFactory::createMethod(const std::string& method)
{
    if (method == "GET")
        return new GET();

    if (method == "POST")
        return new POST();

    if (method == "DELETE")
        return new DeleteMethod();

    return NULL;
}
