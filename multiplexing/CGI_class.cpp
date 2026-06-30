#include "header.hpp"


CGI::CGI(const Client& client, const Location_Config& conf) : client_fd(client.fd), body(client.parsed_request.body){}


void CGI::writeToChild(int fd)
{

}

void CGI::readFromChild(int fd)
{

}

int CGI::execute()
{
    
}