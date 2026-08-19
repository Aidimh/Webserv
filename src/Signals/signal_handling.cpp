#include "header.hpp"


extern volatile sig_atomic_t loop_is_true;

void handle_sigint(int sig)
{
    (void)sig;
    loop_is_true = 0;
}

void handle_sigquit(int sig)
{
    (void)sig;
    loop_is_true = 0;
}

void handle_sigstp(int sig)
{
    (void)sig;
    loop_is_true = 0;
}
