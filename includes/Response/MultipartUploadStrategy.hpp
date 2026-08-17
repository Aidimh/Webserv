#ifndef MultipartUploadStrategy_HPP
#define MultipartUploadStrategy_HPP

#include "Response.hpp"
#include <fstream>
#include <sstream>
#include <string>
#include <map>
#include <sys/stat.h>

struct PartHeaders
{
    std::string filename;
    std::string name;
    std::string contentType;
};

struct MultipartParsingContext
{
    const std::string& body;
    const std::string& delimiter;
    size_t             nextSearchPosition;
    MultipartParsingContext(const std::string& bodyData, const std::string& delim, size_t startPosition): body(bodyData), delimiter(delim), nextSearchPosition(startPosition) {}
};

class MultipartUploadStrategy
{
    private:
        size_t      findHeaderBlockEnd(const std::string& body, size_t headerStart) const;
        bool        isClosingDelimiter(const std::string& body, size_t delimiterPosition, const std::string& delimiter) const;
        std::string sanitizeFilename(const std::string& filename) const;
        std::string trimSpaces(const std::string& value, char c) const;
        Response    buildErrorResponse(int statusCode, const std::string& reasonPhrase, const std::string& body) const;
        void        parseMultipart(const std::string& body, const std::string& delimiter, const std::string& target, size_t& partCount, size_t& savedFileCount) const;
        Response    buildSummaryResponse(size_t partCount, size_t savedFileCount) const;
        // Response    handleMultipartRequest(const Client& client, const std::string& target)

    public:
        bool        validateRequest(const std::map<std::string, std::string>& headers, std::string& boundary) const;
        bool        isMultipartUpload(const std::map<std::string, std::string>& headers) const;
        Response    handleMultipartUpload(const std::string& body, const std::map<std::string, std::string>& headers, const std::string& target);
        std::string extractBoundary(const std::map<std::string, std::string>& headers) const;
        PartHeaders readPartHeaders(const std::string& body, size_t delimiterPosition, const std::string& delimiter, size_t* headerEndPosition = NULL) const;
        PartHeaders parseHeaderBlock(const std::string& headerBlock) const;
        std::string readlineHeader(const std::string& block, size_t& start) const;
        void        parseContentDisposition(const std::string& line, PartHeaders& headers) const;
        void        parseContentType(const std::string& line, PartHeaders& headers) const;
        std::string extractPartBody(const std::string& body, size_t headerEnd, const std::string& delimiter, size_t& nextPartPosition) const;
        bool        parseNextPart(MultipartParsingContext& ctx, PartHeaders& headers, std::string& body) const;
        bool        saveUploadedFile(const std::string& target, const std::string& filename, const std::string& content) const;
};

#endif
