#include "header.hpp"
#include "Error.hpp"
#include "../Logging/Logging.hpp"



void validate_file()
{
    size_t i = 0;
    int depth = 0;

    while (i < Conf_File::tokens.size())
    {
        if (Conf_File::tokens[i] == "{")
            depth++;
        else if (Conf_File::tokens[i] == "}")
        {
            depth--;
            if (depth < 0)
                throw Error::Left_Brace();
        }
        i++;
    }
    if (depth != 0)
        throw Error::Right_Brace();
}
   