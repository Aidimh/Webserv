#!/usr/bin/env python3

# HTTP Headers
print("Content-Type: text/html\r\n\r\n", end="")

# Response Body
print("<html>")
print("<head><title>CGI Hello World</title></head>")
print("<body>")
print("<h1>Hello, World from Python CGI!</h1>")
print("</body>")
print("</html>")