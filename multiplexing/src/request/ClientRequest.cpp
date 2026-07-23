#include "../../include/request/ClientRequest.hpp"

ClientRequest::ClientRequest() : state(HEADERS), status_code(200){}

ClientRequest::ClientRequest(const ClientRequest& other)
{
    *this = other;
}

ClientRequest& ClientRequest::operator=(const ClientRequest& other)
{
    if (this != &other)
    {
        state           =   other.state;
        method          =   other.method;
        request_path    =   other.request_path;
        cgi_extension   =   other.cgi_extension;
        version         =   other.version;
        headers         =   other.headers;
        cgi             =   other.cgi;
        body            =   other.body;
        status_code     =   other.status_code;
    }
    return *this;
}

ClientRequest::~ClientRequest(){}




const std::string& ClientRequest::getMethod() const {return method;}
const std::string& ClientRequest::getRequestPath() const {return request_path;}
const std::string& ClientRequest::getCgiExtension() const {return cgi_extension;}
const std::string& ClientRequest::getVersion() const {return version;}
const std::string& ClientRequest::getBody() const {return body;}
const std::string& ClientRequest::getCgi() const {return cgi;}
const short ClientRequest::getStatusCode() const {return status_code;}
const std::map<std::string, std::string>& ClientRequest::getHeaders() const  {return headers;}


void ClientRequest::setStatusCode(short StatusCode)
{
    this->status_code = StatusCode;
}

void ClientRequest::CleanUri()
{
    std::string cleanUri = "/";
    
    for (size_t i = 1; i < request_path.length(); i++)
    {
        if (request_path[i] == '/' && request_path[i - 1] == '/')
            continue;
            
        cleanUri += request_path[i];
    }
    request_path = cleanUri;
}

bool ClientRequest::RequestLineValidate(void)
{
    if (method != "GET" && method != "POST" && method != "DELETE")
    {
        status_code = 501;
        state = ERROR_STATE;
        return (false);
    }

    if (request_path.empty() || request_path[0] != '/')
    {
        status_code = 400;
        state = ERROR_STATE;
        return (false);
    }

    if (version != "HTTP/1.1" && version != "HTTP/1.0")
    {
        if (version.find("HTTP/") == 0)
        {
            status_code = 505;
            state = ERROR_STATE;
            return (false);
        }
        else
        {
            status_code = 400;
            state = ERROR_STATE;
            return (false);
        }
    }
    
    return (true);
}

void ClientRequest::RequestLineParser(std::string line)
{
    if (line.empty() || !ValidLine(line))
        return;
    
    size_t start;
    size_t end;
    end = line.find(' ');
    if (end == std::string::npos)
    {
        this->status_code = 400;
        this->state = ERROR_STATE;
        return;
    }
    this->method = line.substr(0, end);

    start = line.find_first_not_of(' ', end);
    end = line.find(' ', start);
    if (start == std::string::npos || end == std::string::npos)
    {
        status_code = 400;
        state = ERROR_STATE;
        return;
    }
    this->request_path = line.substr(start, end - start);

    start = line.find_first_not_of(' ', end);
    if (start == std::string::npos)
    {
        this->status_code = 400;
        this->state = ERROR_STATE;
        return;
    }
    end = line.find_last_not_of(" \r\n", start);
    if (end != std::string::npos && end >= start)
        this->version = line.substr(start, end - start + 1);
    
    if (!RequestLineValidate())
        return; 
    CleanUri();
    
}
std::string	ClientRequest::RemoveFirstLastSpaces(std::string& line)
{
    size_t  begin;
    size_t  finish;

    begin = line.find_first_not_of(" \n\r\t");
    finish = line.find_last_not_of(" \n\r\t");

    if (begin == std::string::npos)
        return ("");
    return (line.substr(begin, finish - begin + 1));
}

void ClientRequest::HeadersParser(std::string headers)
{
    std::string RequestLine;
    size_t      endLine;

    endLine = headers.find("\r\n");
    RequestLine = headers.substr(0, endLine);
    RequestLineParser(RemoveFirstLastSpaces(RequestLine));

    size_t  start;
    size_t  end;

    start   = endLine + 2;
    while (end = headers.find("\r\n", start) != std::string::npos)
    {
        std::string header = headers.substr(start, end - start);
        if (header.empty())
        {
            status_code = 400;
            state = ERROR_STATE;
            return;
        }
        size_t colon = header.find(":");
        
        if (colon != std::string::npos)
        {
            std::string key     = header.substr(0, colon);
            std::string value   = header.substr(colon + 1);
            MyToLower(key);
            this->headers[RemoveFirstLastSpaces(key)] = RemoveFirstLastSpaces(value);
        }
    }
}

bool ClientRequest::CheckTransferEncoding(void)
{
    std::map<std::string, std::string>::iterator it;

    it = headers.find("transfer-encoding");
    return (it != headers.end() && !it->second.empty());
}

bool ClientRequest::CheckContentLength(void)
{
    std::map<std::string, std::string>::iterator it;

    it = headers.find("content-length");
    if (it == headers.end())
        return (false);
    return (it != headers.end() && !it->second.empty());
}

size_t ClientRequest::getContentLength(void)
{
    std::map<std::string, std::string>::iterator it = headers.find("content-length");
    if (it == headers.end())
        return (0);
    
    std::string&    value   = it->second;
    size_t          length  = 0;
    size_t          max     = std::numeric_limits<size_t>::max();

    for (size_t i = 0; i < value.length(); i++)
    {
        if (!isdigit(value[i]))
        {
            status_code = 400;
            state = ERROR_STATE;
            return 0;
        }
        size_t  digit = value[i] - 48;
        if (length > (max - digit) / 10)
        {
            status_code = 400;
            state = ERROR_STATE;
            return 0;
        }
        length = (length * 10) + digit;
    }
    return (length);
}

void ClientRequest::parse(Client& client)
{
    if (this->state == ERROR_STATE)
        return;
    if (client.parsed_request.state == HEADERS)
    {
        if (client.request.length() > 8192)
        {
            this->status_code = 431;
            this->state = ERROR_STATE;
            return;
        }

        size_t begin = removeWhitespace(client);
        if (begin > 0)
            client.request.erase(0, begin);
        if (client.request.empty())
            return;
        
        size_t check;
        check = client.request.find("\r\n\r\n");
        if (check != std::string::npos)
        {
            std::string headers;
            std::string extra;
            
            headers = client.request.substr(0, check + 2);
            HeadersParser(headers);
            if (this->state = ERROR_STATE)
                return;
            state = BODY;
            extra = client.request.substr(check + 4);
            client.request.clear();

            if(CheckTransferEncoding() && CheckContentLength())
            {
                status_code = 400;
                state = ERROR_STATE;
                return;
            }
            else if (CheckTransferEncoding())
                chunks.append(extra);
            else
                body = extra;
            #define max_body_size 9999999999 // i need to take the maxbodysize from my peer 
            if(getContentLength() > max_body_size)
            {
                status_code = 413;
                state = ERROR_STATE;
                return;
            }
        }
    }

}