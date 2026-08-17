#ifndef CLIENTREQUEST_HPP
#define CLIENTREQUEST_HPP

#include  <sys/socket.h>
#include  <sys/stat.h>
#include  <unistd.h>
#include  <sstream>
#include <fcntl.h>
#include <limits>
#include<string>
#include <map>


struct Client;

class ClientRequest
{
	public:
		void setMethod(const std::string& value);
		void setRequestPath(const std::string& value);
		void setBody(const std::string& value);
		void setVersion(const std::string& value);
		void addHeader(const std::string& key, const std::string& value);
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
		size_t										getContentLength(void);
		bool										openTempFile(int ClientFd);
		void										BodyRequest(Client& client);
		void										HandleTransferEncoding(Client& client);
		void										HandleContentLength(Client& client);
		void										reset();
		void										SplitQueryString(void);

		void										removeTempFile();
		const std::string&							getQueryString() const;
		const std::string&							getMethod() const;
    	const std::string&							getRequestPath() const;
    	const std::string&							getCgiExtension() const;
    	const std::string&							getVersion() const;
    	const std::string&							getBody() const;
    	const std::string&							getCgi() const;
    	const std::map<std::string, std::string>&	getHeaders() const;
		short 										getStatusCode() const;
		int											getTmpFileFd() const;
		const std::string&							getTmpFilePath() const; //added
		bool										usesTmpFile() const; // added
		size_t										getBodySize() const;
		size_t										getServerMaxBodySize(Client& client);
		std::string									readBody() const;

		bool										RequestLineTooLong(Client& client);
		void										setTmpFileFd(int newFd);
		void										setStatusCode(short StatusCode);
		void										setBodySize(size_t size);

	private:

		std::string									TmpFilePath;   // add
		std::string									method;
		std::string									request_path;
		std::string									query_string;
		std::string									cgi_extension;
		std::string									version;
		std::map<std::string, std::string>			headers;
		std::string									cgi;
		bool										is_cgi;
		std::string									body;
		short										status_code;
		int											TmpFileFd;
		size_t										BodySize;
		size_t										ContentLength;
		bool										HasContentLength;
		bool										HasTransferEncoding;

};



///////////////////////////// Helper Functions ////////////////////////////

size_t		removeWhitespace(Client& client);
bool		ValidLine(std::string line);
void		MyToLower(std::string &str);
std::string	RemoveFirstLastSpaces(std::string& line);

#endif
