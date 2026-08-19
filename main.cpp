#include "Response/Dispatcher.hpp"
#include "Request/ClientRequest.hpp"
#include "multiplexing/header.hpp"
#include <iostream>
#include <fstream>
#include <cstdlib>
#include "src/Logging/Logging.hpp"

using namespace std;

int server_index = 0;

std::vector<Server_block> Conf_File::Servers;
std::vector<std::string> Conf_File::tokens;


void open_file(std::string filename)
{
    std::ifstream file(filename.c_str());
    if (!file.is_open())
    {
        ERR() << "open_file: open config file failed path=" << filename << ": " << strerror(errno);
        throw Error::FileNotFound();
    }

    std::string content((std::istreambuf_iterator<char>(file)),
                         std::istreambuf_iterator<char>());
    file.close();
    size_t i = 0;
    // std::cout << content << "\n";
    // exit(1);

    while (i < content.size())
    {
        if (isspace(content[i]))
        {
            i++;
            continue;
        }
        if (content[i] == '#')
        {
            while (i < content.size() && content[i] != '\n')
                i++;
            continue;
        }
        if (content[i] == '"')
        {
            std::string word;
            word += content[i];
            i++;
            while (i < content.size() && content[i] != '"')
            {
                word += content[i];
                i++;
            }
            if (i < content.size()) 
            {
                word += content[i];
                i++;
            }
            Conf_File::tokens.push_back(word);
            continue;
        }
        if (content[i] == '{' || content[i] == '}' || content[i] == ';')
        {
            Conf_File::tokens.push_back(std::string(1, content[i]));
            i++;
            continue;
        }
        std::string word;
        while (i < content.size()
                && !isspace(content[i])
                && content[i] != '{'
                && content[i] != '}'
                && content[i] != ';'
                && content[i] != '#')
        {
            word += content[i];
            i++;
        }
        Conf_File::tokens.push_back(word);
    }

    if (Conf_File::tokens.empty())
        throw Error::EmptyConfig();
    DEBUG("ConfFile") << "open_file: tokenized config path=" << filename
                      << " into " << Conf_File::tokens.size() << " tokens";
}

void initDebug(string &className)
{
	if (className.empty())
	{
		INFO() << "Global Debug (-d) enabled.";
		Logging::EnableDebug("");
	}
	else
	{
		INFO() << "Debug (-d) enabled for class: " << className;
		Logging::EnableDebug(className);
	}
}

void initDetailedDebug(string &className)
{
	if (className.empty())
	{
		INFO() << "Global Detailed Debug (-D) enabled";
		Logging::EnableDetailDebug("");
	}
	else
	{
		INFO() << "Detailed Debug (-D) enabled for class: " << className;
		Logging::EnableDetailDebug(className);
	}
}

int ParseLoggingArgs(int argc, char *argv[])
{
	int fileNameIdx = 1;
	for (int i = 1; i < argc; ++i)
	{
		std::string arg = argv[i];
		if (arg.length() >= 2 && arg[0] == '-')
		{
			std::string className = arg.substr(2);

			if (arg[1] == 'd')
			{
				initDebug(className);
			}
			else if (arg[1] == 'D')
			{
				initDetailedDebug(className);
			}
			fileNameIdx++;
		}
		else
			break;
	}
	return fileNameIdx;
}

int main(int ac , char **av, char **envp)
{
    signal(SIGPIPE, SIG_IGN);
    signal(SIGINT, handle_sigint);
    signal(SIGQUIT, handle_sigquit);
    signal(SIGTSTP, handle_sigstp);
	Logging logger("webserv.log");
    try
    {
		int fileNameIdx = ParseLoggingArgs(ac, av);
        if (ac != fileNameIdx + 1)
            throw Error::Argc();
        if (!av || av[fileNameIdx][0] == '\0')
            throw Error::Argv();
		
        open_file(av[fileNameIdx]);
		INFO() << "Opening file: " << av[fileNameIdx];
        validate_file();
        parse_config_file();
        int error_nb = every_server_has_listen_port();
        if (error_nb)
        {
            ERR() << "main: missing listen port at server block " << error_nb
                  << ", a server cannot operate without a listen port";
            return ERROR;
        }
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
                INFO() << "main: listening on " << Conf_File::Servers[i].host
                       << ":" << Conf_File::Servers[i].listen_port[j];
                j++;
            }
            i++;
        }
        Mux.run();
        INFO() << "main: server stopped";
    }
    catch(const std::exception& e)
    {
        ERR() << "main: " << e.what();
        return ERROR;
    }
    return SUCESS;
}
