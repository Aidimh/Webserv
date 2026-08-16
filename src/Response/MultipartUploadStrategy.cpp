#include "MultipartUploadStrategy.hpp"
#include "../Logging/Logging.hpp"

#include <sys/stat.h>
#include <iostream>

bool MultipartUploadStrategy::isMultipartUpload(const std::map<std::string, std::string>& headers) const
{
    std::map<std::string, std::string>::const_iterator it = headers.find("content-type");

    if (it == headers.end())
        return false;

    const std::string& contentType = it->second;

    return contentType.find("multipart/form-data") != std::string::npos;
}

bool MultipartUploadStrategy::validateRequest(const std::map<std::string, std::string>& headers, std::string& boundary) const
{
    if (!isMultipartUpload(headers))
    {
        WARN() << "MultipartUploadStrategy::validateRequest: content-type is not multipart/form-data";
        return false;
    }

    boundary = extractBoundary(headers);

    if (boundary.empty())
    {
        WARN() << "MultipartUploadStrategy::validateRequest: multipart request has no boundary";
        return false;
    }

    DEBUG("MultipartUploadStrategy") << "validateRequest: boundary=" << boundary;
    return true;
}

Response MultipartUploadStrategy::buildErrorResponse(int statusCode, const std::string& reasonPhrase, const std::string& body) const
{
    Response response;

    response.setStatusCode(statusCode);
    response.setReasonPhrase(reasonPhrase);
    response.addHeader("content-type", "text/plain");
    response.setBody(body);

    return response;
}

bool MultipartUploadStrategy::parseNextPart(MultipartParsingContext& ctx,PartHeaders& headers,std::string& body) const
{
    size_t delimiterPosition = ctx.body.find(ctx.delimiter, ctx.nextSearchPosition);

    if (delimiterPosition == std::string::npos)
        return false;

    if (isClosingDelimiter(ctx.body, delimiterPosition, ctx.delimiter))
        return false;

    size_t headerEnd = std::string::npos;
    headers = readPartHeaders(ctx.body, delimiterPosition, ctx.delimiter, &headerEnd);

    if (headerEnd == std::string::npos)
        return false;

    size_t nextPartPosition = std::string::npos;
    body = extractPartBody(ctx.body, headerEnd, ctx.delimiter, nextPartPosition);

    if (nextPartPosition == std::string::npos)
        return false;

    ctx.nextSearchPosition = nextPartPosition;

    return true;
}

void MultipartUploadStrategy::parseMultipart(const std::string& body, const std::string& delimiter, const std::string& target, size_t& partCount, size_t& savedFileCount) const
{
    MultipartParsingContext ctx(body, delimiter, 0);
    PartHeaders headers;
    std::string partBody;

    partCount = 0;
    savedFileCount = 0;

    while (parseNextPart(ctx, headers, partBody))
    {
        partCount++;
        DDEBUG("MultipartUploadStrategy") << "parseMultipart: part " << partCount
                                          << " name=" << headers.name
                                          << " filename=" << headers.filename
                                          << " size=" << partBody.size() << " bytes";

        if (!headers.filename.empty() && saveUploadedFile(target, headers.filename, partBody))
            savedFileCount++;
    }
    DEBUG("MultipartUploadStrategy") << "parseMultipart: parsed parts=" << partCount
                                     << " saved files=" << savedFileCount
                                     << " target=" << target;
}

Response MultipartUploadStrategy::buildSummaryResponse(size_t partCount, size_t savedFileCount) const
{
    if (partCount == 0)
    {
        WARN() << "MultipartUploadStrategy::buildSummaryResponse: no parts parsed, responding status=400";
        return buildErrorResponse(400, "Bad Request", "Invalid multipart body");
    }
    INFO() << "MultipartUploadStrategy::buildSummaryResponse: saved " << savedFileCount
           << " of " << partCount << " part(s)";

    std::ostringstream summary;
    summary << savedFileCount << " of " << partCount << " part(s) saved";

    Response response;

    response.setStatusCode(201);
    response.setReasonPhrase("Created");
    response.addHeader("content-type", "text/plain");
    response.setBody(summary.str());

    return response;
}

Response MultipartUploadStrategy::handleMultipartUpload(const std::string& body, const std::map<std::string, std::string>& headers, const std::string& target)
{
    std::string boundary;

    if (!validateRequest(headers, boundary))
        return buildErrorResponse(400, "Bad Request", "Not a valid multipart request");

    std::string delimiter = "--" + boundary;
    size_t partCount = 0;
    size_t savedFileCount = 0;

    DEBUG("MultipartUploadStrategy") << "handleMultipartUpload: body_size=" << body.size()
                                     << " bytes target=" << target;
    parseMultipart(body, delimiter, target, partCount, savedFileCount);

    return buildSummaryResponse(partCount, savedFileCount);
}

std::string MultipartUploadStrategy::readlineHeader(const std::string& block, size_t& start) const
{
    if (start >= block.length())
        return "";

    size_t end = block.find("\r\n", start);

    if (end == std::string::npos)
        end = block.length();

    std::string line = block.substr(start, end - start);

    start = end + 2;

    return line;
}

void MultipartUploadStrategy::parseContentDisposition(const std::string& line, PartHeaders& headers) const
{
    const std::string filenameKey = "filename=\"";
    const std::string nameKey = "name=\"";
    size_t filenamePos = line.find(filenameKey);

    if (filenamePos != std::string::npos)
    {
        filenamePos += filenameKey.length();
        size_t filenameEnd = line.find('"', filenamePos);
        headers.filename = line.substr(filenamePos, filenameEnd - filenamePos);
    }

    size_t namePos = line.find(nameKey);

    if (namePos != std::string::npos)
    {
        namePos += nameKey.length();
        size_t nameEnd = line.find('"', namePos);
        headers.name = line.substr(namePos, nameEnd - namePos);
    }
}

void MultipartUploadStrategy::parseContentType(const std::string& line, PartHeaders& headers) const
{
    const std::string contentTypeKey = "content-type:";

    size_t contentTypePos = line.find(contentTypeKey);

    if (contentTypePos != std::string::npos)
    {
        contentTypePos += contentTypeKey.length();

        if (contentTypePos < line.length() && line[contentTypePos] == ' ')
            contentTypePos++;
        headers.contentType = line.substr(contentTypePos);
    }
}

PartHeaders MultipartUploadStrategy::parseHeaderBlock(const std::string& headerBlock) const
{
    PartHeaders headers;

    headers.filename = "";
    headers.name = "";
    headers.contentType = "";

    size_t start = 0;

    while (start < headerBlock.length())
    {
        std::string line = readlineHeader(headerBlock, start);

        if (line.empty())
            break;
        if (line.find("Content-Disposition:") == 0)
            parseContentDisposition(line, headers);
        else if (line.find("content-type:") == 0)
            parseContentType(line, headers);
    }

    return headers;
}

std::string MultipartUploadStrategy::trimSpaces(const std::string& value, char c) const
{
    size_t begin = 0;

    while (begin < value.length() && value[begin] == c)
        begin++;

    size_t end = value.length();

    while (end > begin && value[end - 1] == c)
        end--;

    return value.substr(begin, end - begin);
}

std::string MultipartUploadStrategy::extractBoundary(const std::map<std::string, std::string>& headers) const
{
    std::map<std::string, std::string>::const_iterator it = headers.find("content-type");

    if (it == headers.end())
        return "";

    const std::string& contentType = it->second;

    size_t pos = contentType.find("boundary=");

    if (pos == std::string::npos)
        return "";

    pos += 9;

    while (pos < contentType.length() && contentType[pos] == ' ')
        pos++;

    size_t end = contentType.find(';', pos);

    if (end == std::string::npos)
        end = contentType.length();
    std::string boundary = contentType.substr(pos, end - pos);

    if (boundary.length() >= 2 &&
        boundary[0] == '"' &&
        boundary[boundary.length() - 1] == '"')
    {
        boundary = boundary.substr(1, boundary.length() - 2);
    }

    boundary = trimSpaces(boundary, ' ');

    return boundary;
}

size_t MultipartUploadStrategy::findHeaderBlockEnd(const std::string& body, size_t headerStart) const
{
    return body.find("\r\n\r\n", headerStart);
}

bool MultipartUploadStrategy::isClosingDelimiter(const std::string& body,size_t delimiterPosition,const std::string& delimiter) const
{
    size_t markerPosition = delimiterPosition + delimiter.length();

    if (markerPosition + 2 > body.length())
        return false;

    return body.compare(markerPosition, 2, "--") == 0;
}

PartHeaders MultipartUploadStrategy::readPartHeaders(const std::string& body,size_t delimiterPosition,const std::string& delimiter,size_t* headerEndPosition) const
{
    PartHeaders headers;

    size_t headerStart = delimiterPosition + delimiter.length() + 2;
    size_t headerEnd = findHeaderBlockEnd(body, headerStart);

    if (headerEndPosition != NULL)
        *headerEndPosition = headerEnd;

    if (headerEnd == std::string::npos)
        return headers;

    size_t headerLength = headerEnd - headerStart;
    std::string headerBlock = body.substr(headerStart, headerLength);

    headers = parseHeaderBlock(headerBlock);

    return headers;
}

std::string MultipartUploadStrategy::extractPartBody(const std::string& body,size_t headerEnd,const std::string& delimiter,size_t& nextPartPosition) const
{
    size_t bodyStart = headerEnd + 4;
    std::string boundaryMarker = "\r\n" + delimiter;

    size_t bodyEnd = body.find(boundaryMarker, bodyStart);

    if (bodyEnd == std::string::npos)
    {
        nextPartPosition = std::string::npos;
        return "";
    }

    nextPartPosition = bodyEnd + 2;

    return body.substr(bodyStart, bodyEnd - bodyStart);
}

std::string MultipartUploadStrategy::sanitizeFilename(const std::string& filename) const
{
    size_t slashPos = filename.find_last_of("/\\");
    std::string base = (slashPos == std::string::npos) ? filename : filename.substr(slashPos + 1);

    if (base.empty() || base == "." || base == "..")
        return "";

    return base;
}

bool MultipartUploadStrategy::saveUploadedFile(const std::string& target, const std::string& filename, const std::string& content) const
{
    std::string safeFilename = sanitizeFilename(filename);

    if (safeFilename.empty())
    {
        WARN() << "MultipartUploadStrategy::saveUploadedFile: rejected unsafe filename=" << filename;
        return false;
    }

    std::string path = target;

    if (!path.empty() && path[path.size() - 1] != '/')
        path += '/';

    path += safeFilename;

    struct stat fileInfo;
    if (stat(path.c_str(), &fileInfo) == 0)
    {
        WARN() << "MultipartUploadStrategy::saveUploadedFile: file already exists, not overwriting path=" << path;
        return false;
    }
    std::ofstream outFile(path.c_str(),std::ios::binary | std::ios::trunc);

    if (!outFile.is_open())
    {
        ERR() << "MultipartUploadStrategy::saveUploadedFile: open file failed path=" << path
              << ": " << strerror(errno);
        return false;
    }

    outFile.write(content.data(),
                  static_cast<std::streamsize>(content.size()));

    bool success = outFile.good();

    outFile.close();

    if (success)
        DEBUG("MultipartUploadStrategy") << "saveUploadedFile: wrote " << content.size()
                                         << " bytes to path=" << path;
    else
        ERR() << "MultipartUploadStrategy::saveUploadedFile: write failed path=" << path;

    return success;
}

