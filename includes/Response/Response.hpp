#ifndef RESPONSE_HPP
#define RESPONSE_HPP

#include <string>
#include <map>
#include <sstream>

class Response
{
    private:
        int statusCode;
        std::string reasonPhrase;
        std::map<std::string, std::string> headers;
        std::string body;

    public:

        Response();
        ~Response();

        // void    Response::MethodFactory();
        std::string toString() const;
        void setStatusCode(int code);
        int getStatusCode() const;
        void setReasonPhrase(const std::string& reason);
        const std::string& getReasonPhrase() const;
        void addHeader(const std::string& key, const std::string& value);
        const std::map<std::string,std::string>& getHeaders() const;
        void setBody(const std::string& content);
        const std::string& getBody() const;
};

#endif