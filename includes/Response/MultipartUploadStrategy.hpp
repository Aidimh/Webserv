#ifndef MultipartUploadStrategy_HPP
#define MultipartUploadStrategy_HPP

#include "../Request/ClientRequest.hpp"
#include "Response.hpp"
#include <fstream>
#include <sstream>
#include <string>
#include <sys/stat.h>

struct PartHeaders
{
    std::string filename;
    std::string name;
    std::string contentType;
};

struct MultipartParsingContext
{
    const ClientRequest& request;
    const std::string& delimiter;
    size_t             nextSearchPosition;
    MultipartParsingContext(const ClientRequest& req, const std::string& delim, size_t startPosition): request(req), delimiter(delim), nextSearchPosition(startPosition) {}
};

class MultipartUploadStrategy
{
    private:
        size_t      findHeaderBlockEnd(const ClientRequest& request, size_t headerStart) const;
        bool        isClosingDelimiter(const ClientRequest& request, size_t delimiterPosition, const std::string& delimiter) const;
        std::string sanitizeFilename(const std::string& filename) const;
        std::string trimSpaces(const std::string& value, char c) const;
        Response    buildErrorResponse(int statusCode, const std::string& reasonPhrase, const std::string& body) const;
        void        parseMultipart(const ClientRequest& request, const std::string& delimiter, const std::string& target, size_t& partCount, size_t& savedFileCount) const;
        Response    buildSummaryResponse(size_t partCount, size_t savedFileCount) const;
    public:
        bool        validateRequest(const ClientRequest& request, std::string& boundary) const;
        bool        isMultipartUpload(const ClientRequest& request) const;
        Response    handleMultipartUpload(const ClientRequest& request, const std::string& target) const;
        std::string extractBoundary(const ClientRequest& request) const;
        PartHeaders readPartHeaders(const ClientRequest& request,size_t delimiterPosition,const std::string& delimiter,size_t* headerEndPosition = NULL) const;
        PartHeaders parseHeaderBlock(const std::string& headerBlock) const;
        std::string readlineHeader(const std::string& block, size_t& start) const;
        void        parseContentDisposition(const std::string& line, PartHeaders& headers) const;
        void        parseContentType(const std::string& line, PartHeaders& headers) const;
        std::string extractPartBody(const ClientRequest& request,size_t headerEnd,const std::string& delimiter,size_t& nextPartPosition) const;
        bool        parseNextPart(MultipartParsingContext& ctx,PartHeaders& headers,std::string& body) const;
        bool        saveUploadedFile(const std::string& target, const std::string& filename, const std::string& content) const;
};

#endif

