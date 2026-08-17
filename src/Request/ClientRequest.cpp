#include "../../includes/Request/ClientRequest.hpp"
#include "../../includes/multiplexing/header.hpp"
#include "../Logging/Logging.hpp"
// #define max_body_size 9999999


ClientRequest::ClientRequest() : state(HEADERS), status_code(200), TmpFileFd(-1), BodySize(0), ContentLength(0), HasContentLength(false), HasTransferEncoding(false){}

ClientRequest::ClientRequest(const ClientRequest& other): TmpFileFd(-1)
{
    *this = other;
}

std::string ClientRequest::readBody() const
{
    if (!usesTmpFile())
        return body;

    std::ifstream file(TmpFilePath.c_str(), std::ios::binary);

    std::ostringstream out;
    out << file.rdbuf();

    return out.str();
}

const std::string& ClientRequest::getTmpFilePath() const
{
    return TmpFilePath;
}

bool ClientRequest::usesTmpFile() const
{
    return !TmpFilePath.empty();
}

ClientRequest& ClientRequest::operator=(const ClientRequest& other)
{
    if (this != &other)
    {
		state           =   other.state;
		method          =   other.method;
		request_path    =   other.request_path;
		query_string = other.query_string;
		cgi_extension   =   other.cgi_extension;
		version         =   other.version;
		headers         =   other.headers;
		cgi             =   other.cgi;
		is_cgi          =   other.is_cgi;
		body            =   other.body;
		status_code     =   other.status_code;
		BodySize        =   other.BodySize;
		ContentLength   =   other.ContentLength;
		HasContentLength=   other.HasContentLength;
		HasTransferEncoding = other.HasTransferEncoding;
		TmpFilePath     =   other.TmpFilePath;


		if (TmpFileFd != -1)
		{
			DEBUG("ClientRequest") << "operator=: closed temp file fd=" << TmpFileFd;
			close(TmpFileFd);
			TmpFileFd = -1;
		}
		if (other.TmpFileFd != -1)
		{
			int dupfd = dup(other.TmpFileFd);
			if (dupfd == -1)
			{
				ERR() << "ClientRequest::operator=: dup failed for temp file fd=" << other.TmpFileFd
				      << ": " << strerror(errno);
				TmpFileFd = -1;
			}
			else
			{
				DEBUG("ClientRequest") << "operator=: duplicated temp file fd=" << other.TmpFileFd
				                       << " to fd=" << dupfd;
				TmpFileFd = dupfd;
			}
		}
    }
    return *this;
}

ClientRequest::~ClientRequest(){removeTempFile();}

void ClientRequest::removeTempFile()
{
	if (TmpFileFd != -1)
    {
        DEBUG("ClientRequest") << "~ClientRequest: closed temp file fd=" << TmpFileFd;
        close(TmpFileFd);
        TmpFileFd = -1;
    }
	if (!TmpFilePath.empty())
	{
		unlink(TmpFilePath.c_str());
		TmpFilePath.clear();
	}
}
void ClientRequest::setMethod(const std::string& value){method = value;}
void ClientRequest::setRequestPath(const std::string& value){request_path = value;}
void ClientRequest::setBody(const std::string& value){body = value;}
void ClientRequest::setVersion(const std::string& value){version = value;}
void ClientRequest::addHeader(const std::string& key, const std::string& value){headers[key] = value;}

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
const std::string&	ClientRequest::getQueryString() const{return query_string;}

void ClientRequest::setBodySize(size_t size) {BodySize = size;}
void ClientRequest::setTmpFileFd(int newFd) {this->TmpFileFd = newFd;}
void ClientRequest::setStatusCode(short StatusCode) {this->status_code = StatusCode; }


void ClientRequest::reset()
{
    state = HEADERS;
    method.clear();
    request_path.clear();
	query_string.clear();
    cgi_extension.clear();
    version.clear();
    headers.clear();
    cgi.clear();
    body.clear();
    status_code = 200;
	removeTempFile();
    BodySize = 0;
    ContentLength = 0;
    HasContentLength = false;
    HasTransferEncoding = false;
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

size_t  ClientRequest::getServerMaxBodySize(Client& client)
{
    size_t i = 0;
    for (i = 0; i < Conf_File::Servers.size(); i++)
    {
        if (Conf_File::Servers[i].listen_port[0] == client.port)
            break;
    }
    if (i == Conf_File::Servers.size() || !Conf_File::Servers[i].client_max_body_found)
    {
        DDEBUG("ClientRequest") << "getServerMaxBodySize: no limit configured for port=" << client.port
                                << ", using default=1048576 bytes";
        return(1048576);
    }
    size_t founded = Conf_File::Servers[i].max_body_size;

    DDEBUG("ClientRequest") << "getServerMaxBodySize: port=" << client.port
                            << " max_body_size=" << founded << " bytes";
    return (founded);
}

c
    if (!request_path.empty() && request_path.find("http://") == 0)
    {
        size_t path_start = request_path.find('/', 7);
        if (path_start != std::string::npos)
            request_path = request_path.substr(path_start);
        else
            request_path = "/";
    }
    if (request_path.empty() || request_path[0] != '/')
    {
        WARN() << "ClientRequest::RequestLineValidate: rejected status=400 path is empty or not absolute, path=" << request_path;
        status_code = 400;
        state = ERROR_STATE;
        return (false);
    }

    if (version != "HTTP/1.1" && version != "HTTP/1.0")
    {
        if (version.find("HTTP/") == 0)
        {
            WARN() << "ClientRequest::RequestLineValidate: rejected status=505 unsupported version=" << version;
            status_code = 505;
            state = ERROR_STATE;
            return (false);
        }
        else
        {
            WARN() << "ClientRequest::RequestLineValidate: rejected status=400 malformed version=" << version;
            status_code = 400;
            state = ERROR_STATE;
            return (false);
        }
    }

    DEBUG("ClientRequest") << "RequestLineValidate: accepted method=" << method
                           << " path=" << request_path << " version=" << version;
    return (true);
}

void ClientRequest::SplitQueryString(void)
{
	size_t pos = request_path.find('?');
	if (pos == std::string::npos)
	{
		query_string.clear();
		return;
	}
	query_string = request_path.substr(pos + 1);
	request_path = request_path.substr(0, pos);
}

bool uriHasForbiddenByte(const std::string& uri)
{
    for (size_t i = 0; i < uri.size(); i++)
    {
        if (uri[i] == '\0' || static_cast<unsigned char>(uri[i]) < 0x20)
            return true;
        if (uri[i] != '%' || i + 2 >= uri.size())
            continue;
        if (uri[i + 1] == '0' && uri[i + 2] == '0')
            return true;
    }
    return false;
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
        WARN() << "ClientRequest::RequestLineParser: rejected status=400 request line has no space separator";
        status_code = 400;
        state = ERROR_STATE;
        return;
    }
    second_space = line.find(" ", first_space + 1);
    if (second_space == std::string::npos || second_space == first_space + 1)
    {
        WARN() << "ClientRequest::RequestLineParser: rejected status=400 request line is missing the version field";
        status_code = 400;
        state = ERROR_STATE;
        return;
    }
    method = line.substr(0, first_space);
    request_path = line.substr(first_space + 1, second_space - first_space -1);
	SplitQueryString();
	if (request_path.length() > MAX_URI_SIZE)
	{
		WARN() << "ClientRequest::RequestLineParser: rejected status=414 uri length=" << request_path.length()
		       << " exceeds limit=2048";
		status_code = 414;
		state = ERROR_STATE;
		return;
	}
	if (uriHasForbiddenByte(request_path))
	{
		WARN() << "ClientRequest::RequestLineParser: rejected status=400 uri contains a null or control byte";
		status_code = 400;
		state = ERROR_STATE;
		return;
	}
    size_t  end;
    end = line.find_last_not_of(" \r\n");
    if (end == std::string::npos || end < second_space)
    {
        WARN() << "ClientRequest::RequestLineParser: rejected status=400 truncated request line";
        status_code = 400;
        state = ERROR_STATE;
        return;
    }
    version = line.substr(second_space + 1, end - second_space);
    if (!RequestLineValidate())
        return;
    CleanUri();
    DEBUG("ClientRequest") << "RequestLineParser: parsed method=" << method
                           << " path=" << request_path << " version=" << version;
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
            WARN() << "ClientRequest::HeadersParser: rejected status=400 empty header line";
            status_code = 400;
            state = ERROR_STATE;
            return;
        }
        size_t colon = header.find(":");

        if (colon != std::string::npos)
        {
            if (colon > 0 && header[colon -1] == ' ')
            {
                WARN() << "ClientRequest::HeadersParser: rejected status=400 whitespace before colon in header="
                       << header.substr(0, colon);
                status_code     = 400;
                state           = ERROR_STATE;
                return;
            }
            std::string key     = header.substr(0, colon);
            std::string value   = header.substr(colon + 1);
            MyToLower(key);
            key = RemoveFirstLastSpaces(key);
			for (size_t i = 0; i < key.length(); i++)
			{
				if (isspace(key[i]))
				{
					WARN() << "ClientRequest::HeadersParser: rejected status=400 whitespace inside header name=" << key;
					status_code = 400;
					state = ERROR_STATE;
					return;
				}
			}
            if ((key == "content-length" || key == "transfer-encoding") && this->headers.find(key) != this->headers.end())
            {
                WARN() << "ClientRequest::HeadersParser: rejected status=400 duplicate header=" << key;
                status_code = 400;
                state = ERROR_STATE;
                return;
            }
            value = RemoveFirstLastSpaces(value);
            this->headers[key] = value;
            DDEBUG("ClientRequest") << "HeadersParser: header " << key << "=" << value;
			if (key == "content-length")
			{
				HasContentLength = true;
				ContentLength = getContentLength();
			}
			if (key == "transfer-encoding")
				HasTransferEncoding = CheckTransferEncoding();
        }
        else
        {
            WARN() << "ClientRequest::HeadersParser: rejected status=400 header line has no colon";
            status_code = 400;
            state = ERROR_STATE;
            return;
        }
        start = end + 2;
    }
    if (this->headers.find("host") == this->headers.end())
    {
        WARN() << "ClientRequest::HeadersParser: rejected status=400 missing Host header";
        status_code = 400;
        state = ERROR_STATE;
        return;
    }
    DEBUG("ClientRequest") << "HeadersParser: parsed " << this->headers.size() << " headers";
}

bool ClientRequest::CheckTransferEncoding(void)
{
    std::map<std::string, std::string>::iterator it;

    it = headers.find("transfer-encoding");
    if (it != headers.end() && !it->second.empty())
    {
		if (this->version == "HTTP/1.0")
        {
            status_code = 400;
            state = ERROR_STATE;
            return false;
        }
        std::string value = RemoveFirstLastSpaces(it->second);
        MyToLower(value);
        if (value == "chunked")
        {
            DEBUG("ClientRequest") << "CheckTransferEncoding: chunked encoding detected";
            return (true);
        }
        else
        {
            WARN() << "ClientRequest::CheckTransferEncoding: rejected status=501 unsupported transfer-encoding=" << value;
            status_code = 501;
            state = ERROR_STATE;
            return false;
        }
    }
    return (false);
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
            WARN() << "ClientRequest::getContentLength: rejected status=400 non numeric content-length=" << value;
            status_code = 400;
            state = ERROR_STATE;
            return 0;
        }
        size_t  digit = value[i] - 48;
        if (length > (max - digit) / 10)
        {
            WARN() << "ClientRequest::getContentLength: rejected status=400 content-length overflows, value=" << value;
            status_code = 400;
            state = ERROR_STATE;
            return 0;
        }
        length = (length * 10) + digit;
    }
    return (length);
}

bool ClientRequest::RequestLineTooLong(Client& client)
{
    size_t lineEnd = client.request.find("\r\n");
    size_t length = (lineEnd == std::string::npos) ? client.request.size() : lineEnd;

    if (length <= MAX_REQUEST_LINE_SIZE)
        return false;
    WARN() << "ClientRequest::RequestLineTooLong: rejected status=414 request line length="
           << length << " exceeds limit=" << MAX_REQUEST_LINE_SIZE << " fd=" << client.fd;
    status_code = 414;
    state = ERROR_STATE;
    return true;
}

void ClientRequest::parse(Client& client)
{
    if (this->state == ERROR_STATE)
	{
		client.request.clear();
		return;
	}
	if (this->state != HEADERS)
		return;
	size_t begin = removeWhitespace(client);
	if (begin > 0)
		client.request.erase(0, begin);

	if (client.request.empty() || RequestLineTooLong(client))
		return;

	size_t check = client.request.find("\r\n\r\n");
	if (check == std::string::npos)
	{
		if (client.request.length() > MAX_HEADER_SIZE)
		{
			WARN() << "ClientRequest::parse: rejected status=431 header block exceeds limit=" << MAX_HEADER_SIZE
			       << " with no terminator, fd=" << client.fd;
			this->status_code = 431;
			this->state = ERROR_STATE;
		}
		DDEBUG("ClientRequest") << "parse: headers incomplete, buffered=" << client.request.length()
		                        << " bytes fd=" << client.fd;
		return;
	}

	if (check > MAX_HEADER_SIZE)
	{
		WARN() << "ClientRequest::parse: rejected status=431 header size=" << check
		       << " exceeds limit=" << MAX_HEADER_SIZE << " fd=" << client.fd;
		this->status_code = 431;
		this->state = ERROR_STATE;
		return;
	}
	std::string headers = client.request.substr(0, check + 2);

	if (headers.find('\0') != std::string::npos)
	{
		WARN() << "ClientRequest::parse: rejected status=400 null byte in header block fd=" << client.fd;
		status_code = 400;
		state = ERROR_STATE;
		return;
	}

	HeadersParser(headers);
	if (this->state == ERROR_STATE)
		return;

	bool is_chunked = HasTransferEncoding;
	if(is_chunked && HasContentLength)
	{
		WARN() << "ClientRequest::parse: rejected status=400 both content-length and transfer-encoding present fd="
		       << client.fd;
		status_code = 400;
		state = ERROR_STATE;
		return;
	}

	state = BODY;
	client.request.erase(0, check + 4);
	DEBUG("ClientRequest") << "parse: state HEADERS to BODY, fd=" << client.fd
	                       << " chunked=" << (is_chunked ? "yes" : "no")
	                       << " content_length=" << ContentLength;
}

void ClientRequest::BodyRequest(Client& client)
{
	if (!HasContentLength && !HasTransferEncoding)
	{
		DEBUG("ClientRequest") << "BodyRequest: state BODY to DONE, no body expected fd=" << client.fd;
		state = DONE;
		return;
	}
	if (HasTransferEncoding)
	{
		HandleTransferEncoding(client);
		return;
	}
	HandleContentLength(client);
}

std::string intToString(int n)
{
    std::ostringstream oss;
    oss << n;
    return oss.str();
}

bool ClientRequest::openTempFile(int ClientFd)
{
	struct timeval tv;
	gettimeofday(&tv, NULL);
	
	std::stringstream stream;
	stream << "/tmp/storage_" << getpid() << "_" << ClientFd << "_" << tv.tv_sec << "_" << tv.tv_usec;
	std::string FilePath = stream.str();
	TmpFilePath = FilePath;

	int fd = open(FilePath.c_str(), O_CREAT | O_WRONLY | O_TRUNC, 0600);
	if (fd == -1)
	{
		ERR() << "ClientRequest::openTempFile: open temp file failed path=" << FilePath
		      << ": " << strerror(errno);
		status_code = 500;
		state       = ERROR_STATE;
		TmpFilePath.clear();
		return false;
	}

	TmpFileFd = fd;
	DEBUG("ClientRequest") << "openTempFile: opened temp file fd=" << fd << " path=" << FilePath;
	if (!body.empty())
	{
		write(fd, body.data(), body.length());
		DEBUG("ClientRequest") << "openTempFile: wrote " << body.length()
		                       << " buffered bytes to fd=" << fd;
		std::string().swap(body);
	}

	return true;
}

static size_t chunkedTerminatorEnd(const std::string& buffer, size_t afterSizeLine)
{
    if (buffer.size() < afterSizeLine + 2)
        return std::string::npos;
    if (buffer.compare(afterSizeLine, 2, "\r\n") == 0)
        return afterSizeLine + 2;

    size_t end = buffer.find("\r\n\r\n", afterSizeLine);

    if (end == std::string::npos)
        return std::string::npos;
    return end + 4;
}

void	ClientRequest::HandleTransferEncoding(Client& client)
{
    size_t max_body_size = getServerMaxBodySize(client);
    while(true)
    {
        size_t pos  = client.request.find("\r\n");
        if (pos == std::string::npos)
            return;
        if (pos > 100)
		{
			WARN() << "ClientRequest::HandleTransferEncoding: rejected status=400 chunk size line too long="
			       << pos << " fd=" << client.fd;
			status_code = 400;
			state = ERROR_STATE;
			return;
		}
        std::string	SizeLine	=	client.request.substr(0, pos);
		if (SizeLine.find('-') != std::string::npos)
		{
			WARN() << "ClientRequest::HandleTransferEncoding: rejected status=400 negative chunk size="
			       << SizeLine << " fd=" << client.fd;
			status_code = 400;
			state = ERROR_STATE;
			return;
		}
        size_t	semicolon	=	SizeLine.find(";");
		if (semicolon != std::string::npos)
			SizeLine = SizeLine.substr(0, semicolon);
		
		size_t				ChunkSize = 0;
		std::stringstream	ss;

		ss << std::hex << SizeLine;
		ss >> ChunkSize;

		if (ss.fail() || !ss.eof())
		{
			WARN() << "ClientRequest::HandleTransferEncoding: rejected status=400 malformed chunk size="
			       << SizeLine << " fd=" << client.fd;
			status_code = 400;
			state		= ERROR_STATE;
			return;
		}
		size_t	needs = pos + 2 + ChunkSize + 2;
		if (ChunkSize != 0 && client.request.length() < needs)
		{
			DDEBUG("ClientRequest") << "HandleTransferEncoding: chunk incomplete, have="
			                        << client.request.length() << " need=" << needs << " fd=" << client.fd;
			return;
		}
		if (ChunkSize != 0 && client.request.substr(pos + 2 + ChunkSize, 2) != "\r\n")
		{
			WARN() << "ClientRequest::HandleTransferEncoding: rejected status=400 chunk not terminated by CRLF fd="
			       << client.fd;
			status_code = 400;
			state = ERROR_STATE;
			return;
		}

		if (ChunkSize == 0)
		{
			size_t end = chunkedTerminatorEnd(client.request, pos + 2);
			if (end == std::string::npos)
			{
				DDEBUG("ClientRequest") << "HandleTransferEncoding: waiting for the trailer section fd="
				                        << client.fd;
				return;
			}
			if (TmpFileFd != -1)
			{
				DEBUG("ClientRequest") << "HandleTransferEncoding: closed temp file fd=" << TmpFileFd;
				close(TmpFileFd);
				TmpFileFd = -1;
			}
			client.request.erase(0, end);
			state = DONE;
			DEBUG("ClientRequest") << "HandleTransferEncoding: state BODY to DONE, received body_size="
			                       << BodySize << " bytes fd=" << client.fd;
			return;
		}
		if (BodySize + ChunkSize > max_body_size)
		{
			WARN() << "ClientRequest::HandleTransferEncoding: rejected status=413 body_size="
			       << (BodySize + ChunkSize) << " exceeds max_body_size=" << max_body_size
			       << " fd=" << client.fd;
			if (TmpFileFd != -1)
			{
				DEBUG("ClientRequest") << "HandleTransferEncoding: closed temp file fd=" << TmpFileFd;
				close (TmpFileFd);
				TmpFileFd = -1;
			}
			std::string().swap(body);
			status_code = 413;
			state = ERROR_STATE;
			return;
		}
		if (this->method == "GET" || this->method == "DELETE")
		{
			BodySize += ChunkSize;
			client.request.erase(0, needs);
			continue;
		}
		std::string	chunk = client.request.substr(pos + 2, ChunkSize);
		if (BodySize + ChunkSize <= MAX_RAM_BUFFER)
		{
			body.append(chunk);
			BodySize += ChunkSize;
		}
		else
		{
			if (TmpFileFd == -1)
			{
				if (!openTempFile(client.fd))
					return;
			}
			ssize_t written = write(TmpFileFd, chunk.data(), ChunkSize);
			if (written < 0 || (size_t)written != ChunkSize)
			{
				ERR() << "ClientRequest::HandleTransferEncoding: write to temp file fd=" << TmpFileFd
				      << " failed, wrote=" << written << " of " << ChunkSize << ": " << strerror(errno);
				status_code = 500;
				state = ERROR_STATE;
				return;
			}
			DDEBUG("ClientRequest") << "HandleTransferEncoding: wrote " << written
			                        << " bytes to temp file fd=" << TmpFileFd;
			BodySize += written;
		}
		client.request.erase(0, needs);
    }
}

void ClientRequest::HandleContentLength(Client& client)
{
    size_t max_body_size = getServerMaxBodySize(client);

	if (getContentLength() > max_body_size)
	{
		WARN() << "ClientRequest::HandleContentLength: rejected status=413 content_length=" << getContentLength()
		       << " exceeds max_body_size=" << max_body_size << " fd=" << client.fd;
		if (TmpFileFd != -1)
		{
			DEBUG("ClientRequest") << "HandleContentLength: closed temp file fd=" << TmpFileFd;
			close(TmpFileFd);
			TmpFileFd = -1;
		}
		std::string().swap(body);
		status_code = 413;
		state = ERROR_STATE;
		return;
	}

	if (BodySize >= getContentLength())
	{
		if (TmpFileFd != -1)
		{
			DEBUG("ClientRequest") << "HandleContentLength: closed temp file fd=" << TmpFileFd;
			close(TmpFileFd);
			TmpFileFd = -1;
		}
		state = DONE;
		DEBUG("ClientRequest") << "HandleContentLength: state BODY to DONE, received body_size="
		                       << BodySize << " bytes fd=" << client.fd;
		return;
	}

	size_t remaining = getContentLength() - BodySize;
	size_t bytesToTake = (client.request.length() > remaining) ? remaining : client.request.length();

	if (this->method == "GET" || this->method == "DELETE")
	{
		BodySize += bytesToTake;
		client.request.erase(0, bytesToTake);
		if (BodySize >= getContentLength())
			state = DONE;
		return;
	}

	if (getContentLength() <= MAX_RAM_BUFFER)
	{
		body.append(client.request, 0, bytesToTake);
		BodySize += bytesToTake;
	}
	else
	{
		if (TmpFileFd == -1)
		{
			if (!openTempFile(client.fd))
				return;
		}

		ssize_t written = write(TmpFileFd, client.request.data(), bytesToTake);
		if (written < 0)
		{
			ERR() << "ClientRequest::HandleContentLength: write to temp file fd=" << TmpFileFd
			      << " failed: " << strerror(errno);
			status_code = 500;
			state = ERROR_STATE;
			return;
		}
		DDEBUG("ClientRequest") << "HandleContentLength: wrote " << written
		                        << " bytes to temp file fd=" << TmpFileFd;
		bytesToTake = written;
		BodySize += written;
	}
	client.request.erase(0, bytesToTake);
	DDEBUG("ClientRequest") << "HandleContentLength: body progress " << BodySize
	                        << "/" << getContentLength() << " bytes fd=" << client.fd;

	if (BodySize >= getContentLength())
	{
		if (TmpFileFd != -1)
		{
			DEBUG("ClientRequest") << "HandleContentLength: closed temp file fd=" << TmpFileFd;
			close(TmpFileFd);
			TmpFileFd = -1;
		}
		state = DONE;
		DEBUG("ClientRequest") << "HandleContentLength: state BODY to DONE, received body_size="
		                       << BodySize << " bytes fd=" << client.fd;
	}

}