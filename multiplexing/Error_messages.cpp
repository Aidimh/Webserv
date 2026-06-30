#include "Error.hpp"

const char* Error::Argv::what() const throw()
{
    return "Error!\nNo Config file was provided!.";
}

const char* Error::Argc::what() const throw()
{
    return "Error!\nProgram Expects only 1 argument!.";
}

const char* Error::Bind::what() const throw()
{
    return "Error!\nBind failed!.";
}

const char* Error::Listen_port::what() const throw()
{
    return "Error\nListen port is invalid!.";
}

const char* Error::Socket::what() const throw() 
{
    return "Error!\nSocket failed!.";
}

const char* Error::Listen::what() const throw()
{
    return "Error!\nListen failed!.";
}

const char* Error::auto_Index::what() const throw()
{
    return "Error\nAuto index value must be either on or off!.";
}

const char* Error::FileNotFound::what() const throw ()
{
    return "Error\nConfig file is not found!.";
}

const char* Error::Accept::what() const throw()
{
    return "Error!\nAccept failed!.";
}

const char* Error::Recv::what() const throw()
{
    return "Error!\nRecv failed!.";
}

const char* Error::Poll::what() const throw()
{
    return "Error!\nPoll failed!.";
}

const char* Error::Send::what() const throw()
{
    return "Error\nSend failed!.";
}

const char* Error::Open::what() const throw()
{
    return "Error\nFailed to open config file!";
}

const char* Error::Non_Nemeric_value::what() const throw()
{
    return "Error\nNon Numeric value near!.";
}

const char* Error::SyntaxError::what() const throw()
{
    return "Error\nSyntax Error near directive!.";
}

const char* Error::MaxUploads::what() const throw()
{
    return "Error\nMax upload value was exceeded.";
}

const char* Error::Missing_directive_value::what() const throw()
{
    return "Error\nMissing Directive value!.";
}

const char* Error::CGI_Extension::what() const throw ()
{
    return "Error\nCGI Extension is invalid or not provided!.";
}

const char* Error::Max_port::what() const throw()
{
    return "Error\nMax ports was exceeded!.";
}

const char* Error::Methods::what() const throw()
{
    return "Error\nAllowed methods weren't specified!.";
}

const char* Error::Unkonwn_Directive_value::what() const throw()
{
    return "Error\nUnkown directive value!.";
}

const char* Error::Host_ip::what() const throw()
{
    return "Error\nHost ip is invalid or isn't provided!.";
}

const char* Error::Root::what() const throw()
{
    return "Error\nRoot path isn't provided or is invalid!.";
}

const char* Error::CGI_Path::what() const throw()
{
    return "Error\nCGI Path doesn't exists!.";
}

const char* Error::Index::what() const throw()
{
    return "Error\nIndex file isn't provided or doesn't exists!.";
}

const char* Error::EmptyConfig::what() const throw()
{
    return "Error\nConfig file is empty!.";
}

const char* Error::Server_name::what() const throw()
{
    return "Error\nServer name is invalid or is not provided!.";
}

const char* Error::Right_Brace::what() const throw()
{
    return "Error\nMissing closing brace!.";
}

const char* Error::Left_Brace::what() const throw()
{
    return "Error\nMissing Opening brace!.";
}

const char* Error::Unknown_Directive::what() const throw()
{
    return "Error\nUnknown Directive detected!.";
}

const char* Error::UnexpectedEndOfFile::what() const throw()
{
    return "Error\nUnexpected EOF!.";
}

const char* Error::SemiColon::what() const throw()
{
    return "Error\nExpected ';' after directive value !.";
}

const char* Error::Error_page::what() const throw()
{
    return "Error\nError page is invalid or not provided!.";
}