#!/usr/bin/env python3
import sys

# Print HTTP headers (Note the \r\n for HTTP compliance!)
print("Content-Type: text/html\r\n\r\n", end="")

# Print HTML with embedded video player
print("""<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <title>Video Player</title>
</head>
<body style="background-color: #121212; color: white; text-align: center; font-family: sans-serif;">
    <h1>CGI Video Stream</h1>
    <video width="720" controls autoplay style="border-radius: 8px;">
        <source src="/upload/video.mp4" type="video/mp4">
        Your browser does not support playing this video.
    </video>
</body>
</html>
""")