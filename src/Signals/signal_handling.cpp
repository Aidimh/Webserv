#include "header.hpp"

extern int loop_is_true;

void handle_sigint(int sig)
{
    (void)sig;
    loop_is_true = false;
    std::cout << "\n";
    std::cout << "Ctrl+C detected . politely shuting down the server💯!.\n";
}

void handle_sigquit(int sig)
{
    (void)sig;
    loop_is_true = false;
    std::cout << "\n";
    std::cout << "Ctrl+\\ detected . politely shuting down the server💯!.\n";
}

void handle_sigstp(int sig)
{
    (void)sig;
    loop_is_true = false;
    std::cout << "\n";
    std::cout << "Ctrl+z detected . politely shuting down the server💯!.\n";
}