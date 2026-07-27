#ifndef MultipartUploadStrategy_HPP
#define MultipartUploadStrategy_HPP

#include "HttpRequest.hpp"
#include "Response.hpp"
#include "AMethod.hpp"
#include <fstream>
#include <sstream>
#include <string>


struct PartHeaders
{
    std::string filename;
    std::string name;
    std::string contentType;
};

struct MultipartParsingContext
{
    const HttpRequest& request;
    const std::string& delimiter;
    size_t             nextSearchPosition;

    MultipartParsingContext(const HttpRequest& req, const std::string& delim, size_t startPosition)
        : request(req), delimiter(delim), nextSearchPosition(startPosition) {}
};

class MultipartUploadStrategy
{
    private:
        size_t      findHeaderBlockEnd(const HttpRequest& request, size_t headerStart) const;
        bool        isClosingDelimiter(const HttpRequest& request, size_t delimiterPosition, const std::string& delimiter) const;
        std::string sanitizeFilename(const std::string& filename) const;
        std::string trimSpaces(const std::string& value, char c) const;
        Response    buildErrorResponse(int statusCode, const std::string& reasonPhrase, const std::string& body) const;
        void        parseMultipart(const HttpRequest& request, const std::string& delimiter, const std::string& target, size_t& partCount, size_t& savedFileCount) const;
        Response    buildSummaryResponse(size_t partCount, size_t savedFileCount) const;
    public:
        bool        validateRequest(const HttpRequest& request, std::string& boundary) const;
        bool        isMultipartUpload(const HttpRequest& request) const;
        Response    handleMultipartUpload(const HttpRequest& request, const std::string& target) const;
        std::string extractBoundary(const HttpRequest& request) const;
        PartHeaders readPartHeaders(const HttpRequest& request,size_t delimiterPosition,const std::string& delimiter,size_t* headerEndPosition = NULL) const;
        PartHeaders parseHeaderBlock(const std::string& headerBlock) const;
        std::string readlineHeader(const std::string& block, size_t& start) const;
        void        parseContentDisposition(const std::string& line, PartHeaders& headers) const;
        void        parseContentType(const std::string& line, PartHeaders& headers) const;
        std::string extractPartBody(const HttpRequest& request,size_t headerEnd,const std::string& delimiter,size_t& nextPartPosition) const;
        bool        parseNextPart(MultipartParsingContext& ctx,PartHeaders& headers,std::string& body) const;
        bool        saveUploadedFile(const std::string& target, const std::string& filename, const std::string& content) const;
};

#endif // MultipartUploadStrategy_HPP

// #ifndef MultipartUploadStrategy_HPP
// #define MultipartUploadStrategy_HPP

// #include "HttpRequest.hpp"
// #include "Response.hpp"
// #include "AMethod.hpp"
// #include <fstream>
// #include <sstream>
// #include <string>


// struct PartHeaders
// {
//     std::string filename;
//     std::string name;
//     std::string contentType;
// };

// struct MultipartParsingContext
// {
//     const HttpRequest& request;
//     const std::string& delimiter;
//     size_t             nextSearchPosition;

//     MultipartParsingContext(const HttpRequest& req, const std::string& delim, size_t startPosition)
//         : request(req), delimiter(delim), nextSearchPosition(startPosition) {}
// };

// class MultipartUploadStrategy
// {
//     private:
//         size_t      findHeaderBlockEnd(const HttpRequest& request, size_t headerStart) const;
//         bool        isClosingDelimiter(const HttpRequest& request, size_t delimiterPosition, const std::string& delimiter) const;
//         std::string sanitizeFilename(const std::string& filename) const;
//         std::string trimSpaces(const std::string& value, char c) const;
//     public:
//         bool validateRequest(const HttpRequest& request,std::string& boundary) const;
//         // std::string POST::resolveUploadPath(const Server_block& server,const Location_Config* location) const
//         bool        isMultipartUpload(const HttpRequest& request) const;
//         Response handleMultipartUpload(const HttpRequest& request, const std::string& target) const;
//         std::string extractBoundary(const HttpRequest& request) const;
//         PartHeaders readPartHeaders(const HttpRequest& request,size_t delimiterPosition,const std::string& delimiter,size_t* headerEndPosition = NULL) const;
//         PartHeaders parseHeaderBlock(const std::string& headerBlock) const;
//         std::string readlineHeader(const std::string& block, size_t& start) const;
//         void        parseContentDisposition(const std::string& line, PartHeaders& headers) const;
//         void        parseContentType(const std::string& line, PartHeaders& headers) const;
//         std::string extractPartBody(const HttpRequest& request,size_t headerEnd,const std::string& delimiter,size_t& nextPartPosition) const;
//         bool        parseNextPart(MultipartParsingContext& ctx,PartHeaders& headers,std::string& body) const;
//         bool        saveUploadedFile(const std::string& target, const std::string& filename, const std::string& content) const;
// };

// #endif // MultipartUploadStrategy_HPP

