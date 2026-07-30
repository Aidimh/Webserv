#ifndef CLIENTREQUEST_HPP
#define CLIENTREQUEST_HPP

#include <limits>
#include <sstream>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string>
#include <map>


struct Client;

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
		
		ParseState 									state;
		
		void										parse(Client& client);
		void										HeadersParser(std::string headers);
		void										RequestLineParser(std::string RequestLine);
		bool										RequestLineValidate(void);
		void	    								CleanUri(void);
		bool										CheckTransferEncoding(void);
		bool										CheckContentLength(void);
		size_t										getContentLength(void);
		
		void										BodyRequest(Client& client);
		void										HandleTransferEncoding(Client& client);



		const std::string&							getMethod() const;
    	const std::string&							getRequestPath() const;
    	const std::string&							getCgiExtension() const;
    	const std::string&							getVersion() const;
    	const std::string&							getBody() const;
    	const std::string&							getCgi() const;
    	const std::map<std::string, std::string>&	getHeaders() const;
		short 										getStatusCode() const;
		int											getTmpFileFd() const;
		size_t										getBodySize() const;
		size_t										getServerMaxBodySize(Client& client);


		void										setTmpFileFd(int newFd);
		void										setStatusCode(short StatusCode);
		void										setBodySize(size_t size);

	private:
		std::string									method;
		std::string									request_path;
		std::string									cgi_extension;
		std::string									version;
		std::map<std::string, std::string>			headers;
		std::string									cgi;
		std::string									body;
		std::string									chunks;
		short										status_code;
		int											TmpFileFd;
		size_t										BodySize;
};


///////////////////////////// Helper Functions ////////////////////////////

size_t		removeWhitespace(Client& client);
bool		ValidLine(std::string line);
void		MyToLower(std::string &str);
std::string	RemoveFirstLastSpaces(std::string& line);

#endif
