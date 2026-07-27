<h1 align="center"><b>Webserv</b></h1>

# Introduction 
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
    root /home/mzanana/Desktop/Github/webserv/Multiplexing;
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

#### Response
The web server returns a HTTP based response. The response is divides by three sections also :  
  + Start line : Contain the http version and the status code;   
  + Headers : Like the request it just define the key-value pairs, one of the important information is the content-type, for example `content-type: text/html` so the browser know to expect a text file.   
  + Body: Contain the content of the response, the body could contain any type of data not just text, it could be images, json file, pdf documents
### Routing
Determining which resource to return is what we refer by routing, connecting request with the requested resource that being requested, there is two types of routing, static and dynamic, in webserv project we use just the static : 
  + Static routing: Serving actual files out of a folder, we can point the web server to a specific file inside a directory to return anything that's requested, example `localhost:1337/www/profile.png` 


