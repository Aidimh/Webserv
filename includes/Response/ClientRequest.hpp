#ifndef CLIENTREQUEST_HPP
#define CLIENTREQUEST_HPP

#include <map>
#include <string>
// #include "../../header.hpp"
// #include "RequestHelpers.hpp"
// #include "RequestHelpers.hpp"

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
    	std::string									RemoveFirstLastSpaces(std::string& line);
		bool										CheckTransferEncoding(void);
		bool										CheckContentLength(void);
		Conf_File::Servers&							getServer(Client& client);
		
		void										BodyRequest(Client& client);
		void										HandleTransferEncoding(Client& client);
		
		size_t										getServerMaxBodySize(Client& client);
		
		const std::string&							getMethod() const;
    	const std::string&							getRequestPath() const;
    	const std::string&							getCgiExtension() const;
		size_t										getContentLength(void);
    	const std::string&							getVersion() const;
    	const std::string&							getBody() const;
    	const std::string&							getCgi() const;
    	const std::map<std::string, std::string>&	getHeaders() const;
		short 										getStatusCode() const;
		int											getTmpFileFd() const;
		size_t										getBodySize() const;

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

size_t removeWhitespace(Client& client);
bool ValidLine(std::string line);
void MyToLower(std::string &str);

#endif