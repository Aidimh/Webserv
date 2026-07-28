#include "Multiplexing/Client.hpp"
#include "../../header.hpp"



ClientRequest::ClientRequest() : state(HEADERS), status_code(200), TmpFileFd(-1), BodySize(0){}

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
        TmpFileFd       =   other.TmpFileFd;
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
short ClientRequest::getStatusCode() const {return status_code;}
const std::map<std::string, std::string>& ClientRequest::getHeaders() const  {return headers;}
int ClientRequest::getTmpFileFd() const {return TmpFileFd;}
size_t ClientRequest::getBodySize() const {return BodySize;}

void ClientRequest::setBodySize(size_t size) {BodySize = size;}
void ClientRequest::setTmpFileFd(int newFd) {this->TmpFileFd = newFd;}
void ClientRequest::setStatusCode(short StatusCode) {this->status_code = StatusCode; }

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

size_t  ClientRequest::getServerMaxBodySize(Client& client)
{
    size_t i = 0;
    for (i = 0; i < Conf_File::Servers.size(); i++)
    {
        if (Conf_File::Servers[i].listen_port == client.port)
            break;
    }
    if (i == Conf_File::Servers.size() || !Conf_File::Servers[i].client_max_body_found)
        return(1048576);
    size_t founded = Conf_File::Servers[i].max_body_size;

    if (Conf_File::Servers[i].body_size_is_MB)
        founded *= (1024 * 1024);
    else if (Conf_File::Servers[i].body_size_is_KB)
        founded *= 1024;
    return (founded);
}


bool ClientRequest::RequestLineValidate(void)
{
    if (method != "GET" && method != "POST" && method != "DELETE")
    {
        status_code = 501;
        state = ERROR_STATE;
        return (false);
    }

    if (request_path.find("http://") == 0)
    {
        size_t path_start = request_path.find('/', 7);
        if (path_start != std::string::npos)
            request_path = request_path.substr(path_start);
        else
            request_path = "/";
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
    
    size_t  first_space;
    size_t  second_space;

    first_space = line.find(" ");
    if (first_space == std::string::npos)
    {
        status_code = 400;
        state = ERROR_STATE;
        return;
    }
    second_space = line.find(" ", first_space + 1);
    if (second_space == std::string::npos || second_space == first_space + 1)
    {
        status_code = 400;
        state = ERROR_STATE;
        return;
    }
    method = line.substr(0, first_space);
    request_path = line.substr(first_space + 1, second_space - first_space -1);

    size_t  end;
    end = line.find_last_not_of(" \r\n");
    if (end == std::string::npos || end < second_space)
    {
        status_code = 400;
        state = ERROR_STATE;
        return;
    }
    version = line.substr(second_space + 1, end - second_space);
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
    if (state == ERROR_STATE)
        return;
    
    size_t  start;
    size_t  end;

    start   = endLine + 2;
    while ((end = headers.find("\r\n", start)) != std::string::npos)
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
            if (colon > 0 && header[colon -1] == ' ')
            {
                status_code     = 400;
                state           = ERROR_STATE;
                return;
            }
            std::string key     = header.substr(0, colon);
            std::string value   = header.substr(colon + 1);
            MyToLower(key);
            key = RemoveFirstLastSpaces(key);
            if (key == "content-length" && this->headers.find(key) != this->headers.end())
            {
                status_code = 400;
                state = ERROR_STATE;
                return;
            }
            value = RemoveFirstLastSpaces(value);
            this->headers[key] = value;
        }
        else
        {
            status_code = 400;
            state = ERROR_STATE;
            return;
        }
        start = end + 2;
    }
    if (this->headers.find("host") == this->headers.end())
    {
        status_code = 400;
        state = ERROR_STATE;
        return;
    }  
}

bool ClientRequest::CheckTransferEncoding(void)
{
    std::map<std::string, std::string>::iterator it;

    it = headers.find("transfer-encoding");
    if (it != headers.end() && !it->second.empty())
    {
        if (it->second.find("chunked") == std::string::npos)
        {
            status_code = 501;
            state = ERROR_STATE;
            return false;
        }
        return (true);
    }
    return (false);
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
    if (client.request.find('\0') != std::string::npos)
    {
        status_code = 400;
        state = ERROR_STATE;
        return;
    }

    if (client.parsed_request.state == HEADERS)
    {
        
        if (client.request.length() > MAX_HEADER_SIZE)
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
            if (this->state == ERROR_STATE)
                return;
            state = BODY;
            extra = client.request.substr(check + 4);
            client.request.clear();

            bool is_chunked = CheckTransferEncoding();
            if(is_chunked && CheckContentLength())
            {
                status_code = 400;
                state = ERROR_STATE;
                return;
            }
            if (state == ERROR_STATE)
                return;
            else if (is_chunked)
                chunks.append(extra);
            else
            {
                size_t expected = getContentLength();
                if (extra.length() > expected)
                {
                    body = extra.substr(0, expected);
                    BodySize += expected;
                }
                else
                {
                    body        =   extra;
                    BodySize    +=  extra.length();
                }
            }
            size_t max_body_size = getServerMaxBodySize(client); 
            
            if(getContentLength() > max_body_size)
            {
                status_code = 413;
                state = ERROR_STATE;
                return;
            }
            if (!is_chunked && BodySize >= getContentLength())
            {
                state = DONE;
                return;
            }
        }
    }

}


void	ClientRequest::HandleTransferEncoding(Client& client)
{

}

void    ClientRequest::BodyRequest(Client& client)
{
	char	buffer[65536];

	if (CheckTransferEncoding())
		HandleTransferEncoding(client);
	else
	{
		if (getContentLength()  > max_body_size) //todo : i need here the exact max body size of the server
		{
			if (TmpFileFd != -1)
            {
                close(TmpFileFd);
                TmpFileFd = -1;
            }
            status_code = 413;
            state = ERROR_STATE;
            //EPOLLOUT
            return;
		}

        if (body.length() >= getContentLength())
        {
            if (TmpFileFd != -1)
            {
                close(TmpFileFd);
                TmpFileFd = -1;
            }
            state = DONE;
            //EPOLLOUT
            return;
        }
        ssize_t bytesRead = recv(client.fd, buffer, sizeof(buffer), 0);
        if (bytesRead < 0)
            return;
        if (bytesRead > 0)
        {
            //timeout part
            size_t remaining = getContentLength() - BodySize;
            size_t bytesToTake = (bytesRead > remaining) ? remaining : bytesRead;
            BodySize += bytesToTake;
            if (getContentLength() <= MAX_RAM_BUFFER)
                body.append(buffer, bytesToTake);
            else
            {
                if (TmpFileFd == -1)
                {
                    struct stat meta;
                    if (stat("www/upload", &meta) != 0)
                        mkdir ("www/upload", 0755);
                
                    std::stringstream stream;
                    stream << client.fd;
                    std::string filePath = "www/upload/storage_" + stream.str();
                    int fd = open(filePath.c_str(), O_CREAT | O_WRONLY | O_TRUNC, 0644);
                    if (fd == -1)
                    {
                        status_code = 500;
                        state = ERROR_STATE;
                        return;
                    }
                    TmpFileFd = fd;
                    if (!body.empty())
                    {
                        write(fd, body.c_str(), body.length());
                        std::string().swap(body);
                    }

                }

                ssize_t written = write(TmpFileFd, buffer, bytesToTake);
                if (written < 0)
                {
                    status_code = 500;
                    state = ERROR_STATE;
                    return;
                }
            }
            
            if (BodySize >= getContentLength())
            {
                if (TmpFileFd != -1)
                {
                    close(TmpFileFd);
                    TmpFileFd = -1;
                }
                //EPOLLOUT
                state = DONE;
            }
        }

        else
        {
            if (TmpFileFd != -1)
            {
                close(TmpFileFd);
                TmpFileFd = -1;
            }
        }
	}
}