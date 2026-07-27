#include "header.hpp"
#include "Error.hpp"



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
    
// void validate_file()
// {
//     size_t i = 0;
//     size_t brace = 0;
//     int depth = 0;
//     bool found_open = false;
//     int open_brace = 0;
//     int close_brace = 0;
//     while (i < Conf_File::tokens.size())
//     {
//         if (Conf_File::tokens[i] == "{")
//         {
//             found_open = true;
//             open_brace++;
//             continue;
//         }
//         if (Conf_File::tokens[i] == "}")
//         {
//             if (!found_open)
//                 throw Error::Left_Brace();
//             while(1)
//             {
//                 if (Conf_File::tokens[i] == "}")
//                     close_brace++;
//                 else 
//                     continue;
//             }
//         }
//     }
// }
