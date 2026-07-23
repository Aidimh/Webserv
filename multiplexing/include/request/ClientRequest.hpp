#ifndef CLIENTREQUEST_HPP
#define CLIENTREQUEST_HPP

#include <map>
#include <string>
#include "../../header.hpp"
#include "RequestHelpers.hpp"

class ClientRequest
{
	public:
    	ClientRequest();
    	ClientRequest(const ClientRequest &other);
    	ClientRequest& operator=(const ClientRequest& other);
    	~ClientRequest();
	
		enum ParseState
		{
	    	HEADERS,
    		BODY,
			DONE,
    		ERROR_STATE
		};
		
		ParseState state;
		
		void		parse(Client& client);
		void		HeadersParser(std::string headers);
		void		RequestLineParser(std::string RequestLine);
		bool		RequestLineValidate(void);
		void	    CleanUri(void);
    	std::string	RemoveFirstLastSpaces(std::string& line);
		bool		CheckTransferEncoding(void);
		bool		CheckContentLength(void);
		size_t		getContentLength(void);
		
		const std::string&							getMethod() const;
    	const std::string&							getRequestPath() const;
    	const std::string&							getCgiExtension() const;
    	const std::string&							getVersion() const;
    	const std::string&							getBody() const;
    	const std::string&							getCgi() const;
    	const std::map<std::string, std::string>&	getHeaders() const;
		const short 								getStatusCode() const;

		void setStatusCode(short StatusCode);
		
	private:
		std::string method;
		std::string request_path;
		std::string cgi_extension;
		std::string version;
		std::map<std::string, std::string> headers;
		std::string cgi;
		std::string body;
		std::string chunks;
		short status_code;
};

#endif
