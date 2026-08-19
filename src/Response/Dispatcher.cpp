#include "../../includes/Response/Dispatcher.hpp"
#include "MethodFactory.hpp"
#include "../../includes/multiplexing/header.hpp"
#include "../Logging/Logging.hpp"
// #include "../../includes/Routing/Routing.hpp"

#include <fstream>

void Dispatcher::setErrorPageBody(Response& response, const Server_block& server)
{
    if (response.getStatusCode() < 400)
        return;

    int code = response.getStatusCode();
    std::string filePath;

    std::map<int, std::string>::const_iterator it = server.error_pages.find(code);
    if (it != server.error_pages.end())
    {
        std::string root = server.root;
        if (!root.empty() && root[root.size() - 1] == '/')
            root.erase(root.size() - 1);
        std::string customPath = it->second;
        if (!customPath.empty() && customPath[0] != '/')
            filePath = root + "/" + customPath;
        else
            filePath = root + customPath;
    }
    else
    {
        std::ostringstream defaultPath;
        defaultPath << "www/error_pages/" << code << ".html";
        filePath = defaultPath.str();
    }

    std::ifstream file(filePath.c_str(), std::ios::binary);
    if (!file.is_open())
    {
        std::ostringstream defaultPath;
        defaultPath << "www/error_pages/" << code << ".html";
        filePath = defaultPath.str();
        file.open(filePath.c_str(), std::ios::binary);
    }

    if (!file.is_open())
        return;

    std::ostringstream page;
    page << file.rdbuf();

    response.setBody(page.str());
    response.addHeader("content-type", "text/html");
}


static std::string statusMessage(short code)
{
    switch (code)
    {
        case HTTP_400_BAD_REQUEST:
            return "Bad Request";

        case HTTP_403_FORBIDDEN:
            return "Forbidden";

        case HTTP_404_NOT_FOUND:
            return "Not Found";

        case HTTP_405_METHOD_NOT_ALLOWED:
            return "Method Not Allowed";

        case HTTP_408_REQUEST_TIMEOUT:
            return "Request Timeout";

        case HTTP_409_CONFLICT:
            return "Conflict";

        case HTTP_410_GONE:
            return "Gone";

        case HTTP_411_LENGTH_REQUIRED:
            return "Length Required";

        case HTTP_413_PAYLOAD_TOO_LARGE:
            return "Payload Too Large";

        // status code to implement : 431 
        case HTTP_431_Request_Header_Fields_Too_Large:
            return "Request Header Fields Too Large";
        
        case HTTP_414_URI_TOO_LONG:
            return "URI Too Long";

        case HTTP_415_UNSUPPORTED_MEDIA:
            return "Unsupported Media Type";

        case HTTP_500_INTERNAL_SERVER_ERROR:
            return "Internal Server Error";

        case HTTP_501_NOT_IMPLEMENTED:
            return "Not Implemented";

        case HTTP_502_BAD_GATEWAY:
            return "Bad Gateway";

        case HTTP_504_GATEWAY_TIMEOUT:
            return "Gateway Timeout";

        case HTTP_505_HTTP_VERSION_NOT_SUPPORTED:
            return "HTTP Version Not Supported";

        default:
            return "Unknown Error";
    }
}

Response Dispatcher::dispatch(Client& client,const Server_block& server)
{

    if (client.parsed_request.state == ClientRequest::ERROR_STATE)
    {
        Response response = AMethod::buildErrorResponse(client.parsed_request.getStatusCode(), statusMessage(client.parsed_request.getStatusCode()));
        setErrorPageBody(response, server);
        return response;
    }

    const Location_Config* location = Router::resolveLocation(client.parsed_request.getRequestPath(), server);
    if (!location)
    {
        Response response = AMethod::buildErrorResponse(HTTP_404_NOT_FOUND, "Not Found");
        setErrorPageBody(response, server);
        return response;
    }

    /* check return status */
	if (location->return_value_is_URL_only && !location->return_url.empty())
	{
		Response response;
		response.setStatusCode(301);
		response.setReasonPhrase("Moved Permanently");
		response.addHeader("Location", location->return_url);
		response.setBody("");
		return response;
	}
	
	if (location->return_value_is_code_only && location->return_code > 0)
	{
		Response response = AMethod::buildErrorResponse(location->return_code, statusMessage(location->return_code));
		setErrorPageBody(response, server);
		return response;
	}

	if (location->has_code_and_url)
	{
		std::map<int, std::string>::const_iterator it = location->return_code_and_url.begin();
		if (it != location->return_code_and_url.end())
		{
			Response response;
			response.setStatusCode(it->first);
			response.setReasonPhrase(statusMessage(it->first));
			response.addHeader("Location", it->second);
			response.setBody("");
			return response;
		}
	}

	if (location->has_code_and_path)
	{
		std::map<int, std::string>::const_iterator it = location->return_code_and_path.begin();
		if (it != location->return_code_and_path.end())
		{
			Response response;
			response.setStatusCode(it->first);
			response.setReasonPhrase(statusMessage(it->first));
			response.addHeader("Location", it->second);
			response.setBody("");
			return response;
		}
	}

	if (location->has_code_and_message)
	{
		std::map<int, std::string>::const_iterator it = location->return_code_and_message.begin();
		if (it != location->return_code_and_message.end())
		{
			Response response;
			response.setStatusCode(it->first);
			response.setReasonPhrase(statusMessage(it->first));
			std::string msg = it->second;
			// Remove surrounding quotes if present
			if (!msg.empty() && msg[0] == '"') msg.erase(0, 1);
			if (!msg.empty() && msg[msg.size() - 1] == '"') msg.erase(msg.size() - 1);
			response.setBody(msg);
			response.addHeader("content-type", "text/html");
			return response;
		}
	}
    
    // this method khasha tkon inside this object (location)
    if (!Router::isMethodAllowed(client.parsed_request.getMethod(), *location)) 
    {
        Response response = AMethod::buildErrorResponse(HTTP_405_METHOD_NOT_ALLOWED, "Method Not Allowed");
        response.addHeader("Allow", Router::allowedMethodList(*location));
        setErrorPageBody(response, server);
        return response;
    }

    if (Router::isCGIRequest(client.parsed_request,*location) == true) 
    {
        Response response;
        response.setResponseMode(Response::CGI_RESPONSE);
        client.cgi_started = true;
        return response;
    }

    AMethod* method = MethodFactory::createMethod(client.parsed_request.getMethod());
    if (!method)
    {
        Response res;
        res.setStatusCode(501);
        res.setReasonPhrase("Not Implemented");
        setErrorPageBody(res, server);
        return res;
    }
    Response response = method->execute(client, server);
    delete method;
    setErrorPageBody(response, server);
    return response;
}
