**This project has been created as part of the 42 curriculum by mzanana, salhali, moel-aid.**

## Description

WebServ is a fully functioning HTTP/1.1 web server written entirely from scratch in C++98. The primary goal of this project is to understand the underlying mechanics of the HTTP protocol, non-blocking socket programming, I/O multiplexing, and the architecture of enterprise web servers like NGINX. 

The server is built to handle multiple concurrent client connections efficiently without relying on multithreading or multiprocessing (except for CGI execution). It relies on the `epoll` system call to monitor state changes and stream data asynchronously. WebServ correctly implements standard HTTP methods (GET, POST, DELETE), serves static files, manages virtual hosts, processes chunked and multipart uploads, and securely handles CGI scripts.

## Instructions

### Compilation

You will need a C++ compiler and `make` installed on your system. To compile the project, run the following command at the root of the repository:

```bash
make
```
This will compile the source code and generate the `webserv` executable.

### Execution

The server requires a configuration file to define its operational parameters. You can run the server by providing the path to a `.conf` file:

```bash
./webserv conf.conf
```

If no configuration file is provided, the server will default to standard behaviors or return an error depending on the implementation.

### Configuration

The `.conf` file uses an NGINX-inspired syntax. It allows you to define virtual servers, ports, hostnames, error pages, maximum client body sizes, allowed HTTP methods, return redirections, CGI extensions, and upload storage paths.

## Resources

During the development of this project, the following resources were consulted:
- RFC 7230 to RFC 7235 (HTTP/1.1 Specifications)
- Beej's Guide to Network Programming
- Manual pages for socket programming (`socket`, `bind`, `listen`, `accept`, `recv`, `send`)
- Manual pages for multiplexing (`epoll_create`, `epoll_ctl`, `epoll_wait`)

### AI Usage

Artificial Intelligence was used during the development of this project to assist with the following tasks:
- **Code Auditing and Hardening:** AI was used to analyze the codebase for critical vulnerabilities, such as file descriptor leaks, memory leaks during massive file uploads, and directory traversal vulnerabilities in the CGI routing logic.
- **Edge Case Identification:** AI assisted in simulating extreme edge cases required by the 42 evaluation framework, such as HTTP chunk size buffer overflows and missing header validation.
- **Refactoring:** AI was utilized to help refactor the `return` directive parsing logic and integrate the HTTP `Connection: close` header mechanics for better HTTP/1.1 compliance.
- **Documentation:** AI generated and formatted markdown reports detailing the identified vulnerabilities and helped structure this README file to meet the required guidelines.

## Technical Features

- **I/O Multiplexing:** Built with `epoll` for efficient, non-blocking asynchronous event handling.
- **Virtual Hosting:** Capability to run multiple servers on different ports or different hostnames.
- **Configuration File:** Custom parser for an NGINX-style configuration file.
- **CGI Execution:** Seamless integration with Python, PHP, and bash scripts.
- **Security:** Protections against buffer overflows, file descriptor leaks, and path traversal.
- **Chunked Encoding:** Supports HTTP Transfer-Encoding chunked and massive file uploads.



## Definitions

### Server
The server is central computer or **software program** that processes a requests and delivers data, resources or services to the other device called the client over a network.   

### Web Server
When we enter a website address, the computer sends a request targeting the web server, where all the files are stored of the website as html pages, images and videos.  
request -> web server -> renders the page in the browser.   
Web servers are not just one things, they are mix of a hardware part where the files are stored and the software part which is like the brain, it analyse the request if its valid or not and then send back the right response of the request.     

### HTTP 
The HTTP stands for HyperText Transfer Protocol and it is an application layer protocol.  
Web servers and web clients like chrome, firefox or brave communicate via the HyperText Transfer Protocol (HTTP) which act as the language to communicate over the World Wide Web.    
We should not over complicated it!! HTTP is just a TEXT, the hypertext word means just a clickable text to take us to another document, webpage or a specific section within the same page. 
The HTTP was designed mainly to fetch html documents and sends it to the client.   

# What a web server does
The web server **listen** on a **port** for a **request** via **transport protocol** and it return a **response** verifying the paths by the **routing** step.  
### Listen on a port
When the web server is ON, it just keep waiting for an incoming request, if there is no request, then the web server do nothing but waiting.  
The default network port provided by the operating system for the web server to listen is 80 for http and 443 for https, and we can define it manually by giving it rule to listen only on the port 1337 for example, if any request came to port 1337 our server gonna analyse it, otherwise the web server gonna ignore it.  

## Configuration file
### What ?  
Configuration file is just a plain text usually ending by `.conf`, it is the instruction manual for the web serve software, the C++ code is just a generic **engine** that knows how to move bytes over a network, how to read files and how to speak HTTP.   
The configuration file is the one responsible to tell this engine **what** to do, **where** to listen and **who** is allowed to connect.   

### Why do we use it?  
So we can edit the instruction outside the source code, for example an administrator want to connect using the port `9090` instead of `8080`, if the port was on the source code he should look for it, change the number and recompile the program.   
Using config file he just edit the `.conf` file and restart the program.   

### When do we use it ?
We read the config file **once** during the entire lifespan of the server process, right when we turn ON the server.   
After opening the file we store the values on a specific structure and from that point we only fetch needed data from it.   

### The Hierarchy    
#### `server` context   
We can run multiple websites from the exact c++ program, we just need to define multiple `server` blocks :  
+ Each `server` block represent a virtual server;   
+ Define which IP address and the port to listen on;  
+ Defines the domain name associated with it `server_name`.   

#### `location` context
Inside the `server` block we can find multiple `location` blocks, when the request reaches the correct server, this server looks for the requested URL which is the path like `/images` and find the matching `location` block to decide what to do with it.   

### Example of a config file
```javascript
server
{
    listen 8080;
    host 0.0.0.0;
    server_name localhost:8080;
    root /home/mzanana/Desktop/Github/webserv/multiplexing;
    index index.html;
    client_max_body_size 1000000; 

    location / 
    {
        allowed_methods GET POST DELETE;
        cgi_extension .py /usr/bin/ls;
        # cgi_extension .sh /usr/bb;
    }
    error_page 404 /404.html;
    error_page 500 /500.html;
}
```
### Client / Server Architecture
+ **Client :** on the HTTP is the browser, python or javascript app or any app that makes HTTP request.   
+ **Server :** HTTP Web Server, Apache, NodeJS, Python Tornado or NGINX.    

# Request response cycle
## Request Structure 
### Definition
The request is what the client (browser) want from the server (webserver), the browser sends just a single formatted block of plain text.   

### Anatomy of HTTP Request
  ```HTTP
POST /submit-form HTTP/1.1 \r\n
Host: localhost:8080\r\n
User-Agent: Mozilla/5.0\r\n
Content-Length: 27\r\n
\r\n
username=admin&password=123
  ```
  
  The request structure divides by **four** sections:  
  + **The request line :**  The very first line on all the request coming. Containing three elements:
    + Request method : GET, POST, DELETE, etc.  
    + URI (Uniform Resource Identifiers), used to identify the resource on the web, (/orders/123);  
    + HTTP version.   
  + **Headers :**  From the second line until the empty line, defines key-value pairs separated by colon and space `: `,They describe the metadata of the request, like the `Host`, the client `User-Agent` or the `Content-Length` of the body, etc.      
  + **Empty line**    
  + **Body :**  Contain the data associated with the message, generally used on the `POST` method, usually `GET` or `DELETE` methods don't have body.  might be POST data to send to the server in a request. 
### Transfer Encoding   
HTTP request usually require **content length** of the body on the headers so the server gonna know exactly how many bytes gonna read from the request body received from the socket.   
What if the client is streaming a live video?? The client himself don't know how many bytes the request gonna take, for those cases we use the **transfer encoding**, which is a way of sending only small chunks to the server and the server keep reading the body based on a structure of the transfer encoding which is like this :   
```http
13\r\n
hello world! \r\n
18\r\n
my name is mohamed\r\n
0\r\n
\r\n
```

which is in the form of :  
```http 
{NextChunkSizeInHexadecimal}{\r\n}
{Chunk}{\r\n}
... keeps on the same loop
```

Every single chunk contain three steps exactly which keep in a loop until we got size 0 of the final line, we start by the size of the next chunk in hexadecimal and "\r\n" and then the chunk content forwarded by "\r\n".    
### tringstream in C++
#### Response
The web server returns a HTTP based response. The response is divides by three sections also :  
  + Start line : Contain the http version and the status code;   
  + Headers : Like the request it just define the key-value pairs, one of the important information is the content-type, for example `content-type: text/html` so the browser know to expect a text file.   
  + Body: Contain the content of the response, the body could contain any type of data not just text, it could be images, json file, pdf documents
### Routing
Determining which resource to return is what we refer by routing, connecting request with the requested resource that being requested, there is two types of routing, static and dynamic, in webserv project we use just the static : 
  + Static routing: Serving actual files out of a folder, we can point the web server to a specific file inside a directory to return anything that's requested, example `localhost:1337/www/profile.png`

    # HTTP Request & Method Handling

## Overview

This module is responsible for handling an **already parsed HTTP request**.

The networking layer (`socket`, `bind`, `listen`, `accept`, `recv`) is implemented by another part of the project.

Likewise, parsing raw HTTP bytes into a structured request object is handled by the parser module.

Our responsibility begins once a valid `HttpRequest` object is received.

---

## Global Flow

```text
               Client (Browser)
                      │
                      ▼
              recv(clientFd)
                      │
                      ▼
               HTTP Parser
                      │
                      ▼
                HttpRequest
                      │
                      ▼
                  Routing
                      │
                      ▼
                 MethodFactory
                      │
          ┌───────────┼───────────┐
          │           │           │
         GET         POST      DELETE
          │           │           │
          └───────────┼───────────┘
                      │
                      ▼
                  Response
                      │
                      ▼
           HTTP Response String
                      │
                      ▼
              send(clientFd)
```

---

## Module Responsibilities

This module is responsible for:

- Selecting the correct HTTP method.
- Executing GET requests.
- Executing POST requests.
- Executing DELETE requests.
- Creating HTTP responses.
- Returning the final HTTP response string.

This module is **NOT** responsible for:

- Socket creation
- Receiving data
- HTTP parsing
- Client connection management

---

## Input

The parser provides the following object:

```cpp
class HttpRequest
{
public:
    std::string method;
    std::string path;
    std::string version;

    std::map<std::string, std::string> headers;

    std::string body;
};
```

Example:

```http
POST /upload HTTP/1.1
Host: localhost
Content-Type: multipart/form-data; boundary=----WebKitXYZ
Content-Length: 512

(binary body...)
```

---

## Project Structure

```text
src/

├── http/
│
│   ├── request/
│   │
│   │   ├── HttpRequest.hpp        # Parsed HTTP request
│   │   └── ClientRequest.hpp      # Parser implementation
│   │
│   ├── response/
│   │
│   │   ├── Response.hpp
│   │   ├── Response.cpp
│   │   └── StatusCode.hpp
│   │
│   └── methods/
│       │
│       ├── AMethod.hpp
│       ├── AMethod.cpp
│       │
│       ├── GET.hpp
│       ├── GET.cpp
│       │
│       ├── POST.hpp
│       ├── POST.cpp
│       │
│       ├── DeleteMethod.hpp
│       ├── DeleteMethod.cpp
│       │
│       ├── MultipartUploadStrategy.hpp
│       └── MultipartUploadStrategy.cpp
│
├── routing/
│   ├── Routing.hpp
│   └── Routing.cpp
│
├── cgi/
│
└── socket/
```

---

## Core Components

### HttpRequest

Represents a complete HTTP request after parsing.

Contains:

- HTTP method
- Request path
- HTTP version
- Request headers
- Request body

---

### Response

Represents the HTTP response generated by the server.

Responsibilities:

- Status code
- Reason phrase
- Headers
- Response body
- Serialize the response into an HTTP message

Example:

```http
HTTP/1.1 200 OK
Content-Type: text/html
Content-Length: 25

<html>Hello World</html>
```

---

### AMethod

Abstract base class for all HTTP methods.

```cpp
class AMethod
{
public:
    virtual ~AMethod() {}

    virtual Response execute(const HttpRequest& request) = 0;
};
```

Every HTTP method inherits from this class.

---

### GET

Responsible for:

- Serving files
- Serving directories
- Autoindex generation
- Returning appropriate error pages

Possible responses:

- 200 OK
- 403 Forbidden
- 404 Not Found

---

### POST

Responsible for:

- Saving uploaded data
- Creating new resources
- Handling multipart/form-data uploads
- Delegating multipart parsing to `MultipartUploadStrategy`

Possible responses:

- 201 Created
- 400 Bad Request
- 403 Forbidden
- 500 Internal Server Error

---

### DELETE

Responsible for deleting existing resources.

Possible responses:

- 204 No Content
- 403 Forbidden
- 404 Not Found

---

### MultipartUploadStrategy

Encapsulates multipart/form-data parsing.

Responsibilities:

- Detect multipart requests
- Extract boundary
- Locate multipart delimiters
- Parse part headers
- Extract uploaded file metadata
- Extract file content

This keeps the `POST` implementation focused on request handling while the multipart parsing logic remains isolated.

---

## Design Principles

The module follows the **Single Responsibility Principle (SRP)**.

Each class has a single responsibility:

| Class | Responsibility |
|--------|----------------|
| HttpRequest | Represents a parsed request |
| Routing | Selects the appropriate route |
| MethodFactory | Creates the correct HTTP method |
| GET | Handles GET requests |
| POST | Handles POST requests |
| DELETE | Handles DELETE requests |
| MultipartUploadStrategy | Parses multipart/form-data |
| Response | Builds the HTTP response |

This separation makes the code easier to maintain, extend, and test.

