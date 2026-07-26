#include "include/request/ClientRequest.hpp"
#include "header.hpp"
#include <iostream>

int main()
{
    // 1. Create a fake client
    Client fake_client;
    fake_client.fd = -1; // We aren't using real sockets

    // 2. Simulate a recv() containing headers and the FULL body
    fake_client.request = 
        "POST /../../../upload.html HTTP/1.1\r\n"
        "Host: localhost:8080\r\n"
        "Content-Length: 6\r\n"
        "\r\n"
        "Hello World!";

    // 3. Run your parser
    ClientRequest my_parser;
    my_parser.parse(fake_client);

    // 4. Print the results to verify!
    std::cout << "--- PARSER RESULTS ---\n";
    std::cout << "Method:      " << my_parser.getMethod() << "\n";
    std::cout << "URI:         " << my_parser.getRequestPath() << "\n";
    std::cout << "Version:     " << my_parser.getVersion() << "\n";
    std::cout << "Status Code: " << my_parser.getStatusCode() << "\n";
    std::cout << "State:       " << my_parser.state << " (Expected 2 for DONE)\n";
    std::cout << "Body Size:   " << my_parser.getBodySize() << "\n";
    std::cout << "Body Data:   " << my_parser.getBody() << "\n";

    return 0;
}