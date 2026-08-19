#include "includes/Response/Dispatcher.hpp"
#include "includes/Request/ClientRequest.hpp"
#include "includes/multiplexing/header.hpp"
#include <iostream>
#include <fstream>
#include <cstdlib>
int server_index = 0;

std::vector<Server_block> Conf_File::Servers;
std::vector<std::string> Conf_File::tokens;

int main(int ac , char **av, char **envp)
{
    signal(SIGPIPE, SIG_IGN);
    signal(SIGINT, handle_sigint);
    signal(SIGQUIT, handle_sigquit);
    signal(SIGTSTP, handle_sigstp);
    try
    {
        if (ac != 2)
            throw Error::Argc();
        if (!av || av[1][0] == '\0')
            throw Error::Argv();
		
        open_file(av[1]);
        validate_file();
        parse_config_file();
        int error_nb = every_server_has_listen_port();
        if (error_nb)
            return ERROR;
        size_t i = 0;
        Multiplexer Mux;
        while(i < Conf_File::Servers.size())
        {
            size_t j = 0;
            while (j < Conf_File::Servers[i].ports_count)
            {
                Socket *s = new Socket(Conf_File::Servers[i]);
                Mux.env = envp;
                s->setup(Conf_File::Servers[i].listen_port[j], Conf_File::Servers[i].host);
                Mux.addServer(s);
                j++;
            }
            i++;
        }
        Mux.run();
    }
    catch(const std::exception& e)
    {
        return ERROR;
    }
    return SUCESS;
}
