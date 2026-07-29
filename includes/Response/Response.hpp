#ifndef RESPONSE_HPP
#define RESPONSE_HPP

#include <string>
#include <map>
#include <sstream>

class Response
{
    public:
        enum ResponseMode
        {
            NORMAL_RESPONSE,
            STREAMING_RESPONSE
        };

    private:
        int statusCode;
        std::string reasonPhrase;
        std::map<std::string, std::string> headers;
        std::string body;
        ResponseMode _mode;

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
        void setResponseMode(ResponseMode mode);
        ResponseMode getResponseMode() const;
        bool isStreaming() const;
};

#endif
