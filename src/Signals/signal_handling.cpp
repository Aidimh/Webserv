#include "header.hpp"
#include "../Logging/Logging.hpp"

extern volatile sig_atomic_t loop_is_true;

void handle_sigint(int sig)
{
    (void)sig;
    loop_is_true = 0;
    INFO() << "handle_sigint: SIGINT received, shutting down the server";
}

void handle_sigquit(int sig)
{
    (void)sig;
    loop_is_true = 0;
    INFO() << "handle_sigquit: SIGQUIT received, shutting down the server";
}

void handle_sigstp(int sig)
{
    (void)sig;
    loop_is_true = 0;
    INFO() << "handle_sigstp: SIGTSTP received, shutting down the server";
}
