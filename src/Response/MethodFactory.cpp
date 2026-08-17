#include "MethodFactory.hpp"
#include "../Logging/Logging.hpp"

AMethod* MethodFactory::createMethod(const std::string& method)
{
    // if(allowed_methods)
    DEBUG("MethodFactory") << "createMethod: creating handler for method=" << method;
    if (method == "GET")
        return new GET();

    if (method == "POST")
        return new POST();

    if (method == "DELETE")
        return new DeleteMethod();

    WARN() << "MethodFactory::createMethod: no handler for method=" << method;
    return NULL;
}

