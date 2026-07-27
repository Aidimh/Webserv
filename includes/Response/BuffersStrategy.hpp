#ifndef BUFFERSSTRATEGY_HPP
#define BUFFERSSTRATEGY_HPP

#include "AMethod.hpp"
#include "Response.hpp"
#include "Request.hpp"
class BuffersStrategy
{
    public:
        void appendReceivedData(const std::string& data);
        bool hasCompleteRequest() const;
        std::string extractRequest();
        void setResponse(const std::string& response);
        std::string getPendingResponse() const;
        void consumeSentBytes(size_t sent);

    private:
        std::string receiveBuffer;
        std::string sendBuffer;
};

#endif // BUFFERSSTRATEGY_HPP