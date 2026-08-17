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


///////////////////////////////////// print debug ////////////////////////////////////////////



#include <iostream>
#include <vector>
#include <map>

// Helper lambda/function to print string vectors conveniently
static void printStringVector(const std::string& label, const std::vector<std::string>& vec, const std::string& indent = "  ") {
    std::cout << indent << label << ": [ ";
    for (size_t i = 0; i < vec.size(); ++i) {
        std::cout << "\"" << vec[i] << "\"" << (i + 1 < vec.size() ? ", " : " ");
    }
    std::cout << "]\n";
}

// Helper lambda/function to print error_pages maps
static void printErrorPages(const std::map<int, std::string>& error_pages, const std::string& indent = "  ") {
    std::cout << indent << "Error Pages:\n";
    if (error_pages.empty()) {
        std::cout << indent << "  (none)\n";
        return;
    }
    for (std::map<int, std::string>::const_iterator it = error_pages.begin(); it != error_pages.end(); ++it) {
        std::cout << indent << "  Code " << it->first << " -> " << it->second << "\n";
    }
}

void printConfig()
{
    std::cout << "===========================================\n";
    std::cout << "        PARSED CONFIGURATION SUMMARY       \n";
    std::cout << "===========================================\n";
    std::cout << "Total Server Blocks: " << Conf_File::Servers.size() << "\n\n";

    for (size_t i = 0; i < Conf_File::Servers.size(); ++i) {
        const Server_block& server = Conf_File::Servers[i];

        std::cout << "-------------------------------------------\n";
        std::cout << " SERVER BLOCK #" << (i + 1) << "\n";
        std::cout << "-------------------------------------------\n";
        std::cout << "  Host:             " << (server.host.empty() ? "(none)" : server.host) << "\n";
        std::cout << "  Server Name:      " << (server.server_name.empty() ? "(none)" : server.server_name) << "\n";
        std::cout << "  Root:             " << (server.root.empty() ? "(none)" : server.root) << "\n";
        std::cout << "  Autoindex:        " << (server.server_auto_index.empty() ? "(none)" : server.server_auto_index) << "\n";
        std::cout << "  Max Body Size:    " << server.max_body_size << "\n";
        
        // Listen Ports
        std::cout << "  Listen Ports:     [ ";
        for (size_t p = 0; p < server.listen_port.size(); ++p) {
            std::cout << server.listen_port[p] << (p + 1 < server.listen_port.size() ? ", " : " ");
        }
        std::cout << "]\n";

        // Index Files & Methods
        printStringVector("Index Files", server.index_files);
        printStringVector("Allowed Methods", server.methods);

        // Error Pages
        printErrorPages(server.error_pages);

        // Flags Status
        std::cout << "  Flags:\n";
        std::cout << "    [server_found: " << std::boolalpha << server.server_found
                  << " | host_found: " << server.host_found
                  << " | root_found: " << server.root_found
                  << " | listen_found: " << server.listen_found << "]\n";

        std::cout << "\n  --- Location Blocks (" << server.location.size() << ") ---\n";
        for (size_t j = 0; j < server.location.size(); ++j) {
            const Location_Config& loc = server.location[j];

            std::cout << "\n    [Location #" << (j + 1) << "]\n";
            std::cout << "      Path:            " << (loc.path.empty() ? "(none)" : loc.path) << "\n";
            std::cout << "      Root:            " << (loc.root.empty() ? "(none)" : loc.root) << "\n";
            std::cout << "      Upload Path:     " << (loc.upload_path.empty() ? "(none)" : loc.upload_path) << "\n";
            std::cout << "      Return/Redirect: " << (loc._return.empty() ? "(none)" : loc._return) << "\n";
            std::cout << "      Autoindex:       " << (loc.autoindex.empty() ? "(none)" : loc.autoindex) << "\n";
            std::cout << "      Max Body Size:   " << loc.max_body_size << "\n";

            printStringVector("Index Files", loc.index_files, "      ");
            printStringVector("Allowed Methods", loc.allowed_methods, "      ");
            printStringVector("CGI Extensions", loc.cgi_extensions, "      ");
            printStringVector("CGI Paths", loc.cgi_paths, "      ");
            printErrorPages(loc.error_pages, "      ");
        }
        std::cout << "\n";
    }
    std::cout << "===========================================\n";
}



///////////////////////////////////// print debug ////////////////////////////////////////////



////// 12 : here we handle also SIGPIPE ///////////////////////////////////

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
		Logging logger("webserv.log");
		int fileNameIdx = ParseLoggingArgs(ac, av);
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
                Socket *s = new Socket();
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


// int main(int ac , char **av, char **envp)
// {
//     signal(SIGINT, handle_sigint);
//     signal(SIGQUIT, handle_sigquit);
//     signal(SIGTSTP, handle_sigstp);
//     try
//     {
// 		Logging logger("webserv.log");
// 		int fileNameIdx = ParseLoggingArgs(ac, av);
// 		// if (ac != 2)
//         //     throw Error::Argc();
//         // if (!av || av[1][0] == '\0')
//         //     throw Error::Argv();
//         open_file(av[fileNameIdx]);
// 		INFO() << "Opening file: " << av[fileNameIdx];
//         validate_file();
//         parse_config_file();
//         int error_nb = every_server_has_listen_port();
//         if (error_nb)
//         {
//             ERR() << "main: missing listen port at server block " << error_nb
//                   << ", a server cannot operate without a listen port";
//             return ERROR;
//         }
//         // printConfig();
//         // exit(1);
//         size_t i = 0;
//         Multiplexer Mux;
//         while(i < Conf_File::Servers.size())
//         {
//             size_t j = 0;
//             while (j < Conf_File::Servers[i].ports_count)
//             {
//                 Socket *s = new Socket();
//                 Mux.env = envp;
//                 s->setup(Conf_File::Servers[i].listen_port[j], Conf_File::Servers[i].host);
//                 Mux.addServer(s);
//                 INFO() << "main: listening on " << Conf_File::Servers[i].host
//                        << ":" << Conf_File::Servers[i].listen_port[j];
//                 j++;
//             }
//             i++;
//         }
//         Mux.run();
//         INFO() << "main: server stopped";
//     }
//     catch(const std::exception& e)
//     {
//         ERR() << "main: " << e.what();
//     }
//     return 0;
// }
