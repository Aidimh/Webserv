#include "Response.hpp"

Response::Response()
    : statusCode(0), reasonPhrase(""), body("")
{
}

Response::~Response()
{
}

void    Response::setStatusCode(int code)
{
    statusCode = code;
}

int     Response::getStatusCode() const
{
    return statusCode;
}

void    Response::setReasonPhrase(const std::string& reason)
{
    reasonPhrase = reason;
}

const std::string& Response::getReasonPhrase() const
{
    return reasonPhrase;
}

void    Response::addHeader(const std::string& key, const std::string& value)
{
    headers[key] = value;
}

const std::map<std::string, std::string>& Response::getHeaders() const
{
    return headers;
}

void Response::setBody(const std::string& content)
{
    body = content;

    std::ostringstream oss;
    oss << body.size();

    headers["Content-Length"] = oss.str();
}

const std::string& Response::getBody() const
{
    return body;
}

std::string Response::toString() const
{
    std::ostringstream out;

    out << "HTTP/1.1 "
        << statusCode
        << " "
        << reasonPhrase
        << "\r\n";

    for (std::map<std::string, std::string>::const_iterator it = headers.begin();
         it != headers.end();
         ++it)
    {
        out << it->first
            << ": "
            << it->second
            << "\r\n";
    }

    out << "\r\n";
    out << body;

    return out.str();
}

