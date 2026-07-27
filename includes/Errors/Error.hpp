#pragma once
#include <exception>

class Error
{
    public:
        class Argv : public std::exception
        {
            public:
                const char* what() const throw();
        };
        class Argc : public std::exception
        {
            public:
                const char* what() const throw();
        };
        class FileNotFound : public std::exception
        {
            public:
                const char* what() const throw();
        };
        class EmptyConfig : public std::exception
        {
            public:
                const char* what() const throw();
        };
        class Right_Brace : public std::exception
        {
            public :
                const char* what() const throw();
        };
        class auto_Index : public std::exception
        {
            public :
                const char* what() const throw();
        };
        class Left_Brace : public std::exception
        {
            public:
                const char* what() const throw();
        };
        class Unknown_Directive : public std::exception
        {
            public:
                const char* what() const throw();
        };
        class UnexpectedEndOfFile : public std::exception
        {
            public:
                const char* what() const throw();
        };
        class Listen_port : public std::exception
        {
            public:
                const char* what() const throw();
        };
        class SemiColon :public std::exception
        {
            public:
                const char* what() const throw();
        };
        class Bind : public std::exception
        {
            public:
                const char* what() const throw();
        };
        class Socket : public std::exception
        {
            public:
                const char* what() const throw();
        };
        class Listen : public std::exception
        {
            public:
                const char* what() const throw();
        };
        class Accept : public std::exception
        {
            public:
                const char* what() const throw();
        };
        class Recv : public std::exception
        {
            public:
                const char* what() const throw();
        };
        class Poll : public std::exception
        {
            public:
                const char* what() const throw();
        };
        class Send : public std::exception
        {
            public:
                const char* what() const throw();
        };
        class Open : public std::exception
        {
            public:
                const char* what() const throw();
        };
        class Non_Nemeric_value : public std::exception
        {
            public:
                const char* what() const throw();
        };
        class SyntaxError : public std::exception
        {
            public:
                const char* what() const throw ();
        };

        class Unkonwn_Directive_value : public std::exception
        {
            public :
                const char* what() const throw();
        };
        class MaxUploads : public std::exception
        {
            public:
                const char* what() const throw();
        };
        class Missing_directive_value : public std::exception
        {
            public:
                const char* what() const throw();
        };
        class Methods : public std::exception
        {
            public :
                const char* what() const throw();
        };
        class Max_port : public std::exception
        {
            public:
                const char* what() const throw();
        };
        class CGI_Extension : public std::exception
        {
            public :
                const char* what() const throw ();
        };
        class CGI_Path : public std::exception
        {
            public:
                const char* what() const throw();
        };
        class Host_ip : public std::exception
        {
            public:
                const char* what() const throw();
        };
        class Root :public std::exception
        {
            public:
                const char* what() const throw();
        };
        class Index : public std::exception
        {
            public :
                const char* what() const throw();
        };
        class Server_name : public std::exception
        {
            public:
                const char* what() const throw();
        };
        class Error_page : public std::exception
        {
            public:
                const char* what() const throw();
        };
};