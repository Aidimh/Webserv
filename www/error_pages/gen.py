pages = {
    200: ("OK", "The request has succeeded and the server returned the requested data."),
    201: ("Created", "The request succeeded and a new resource was successfully created on the server."),
    204: ("No Content", "The server successfully processed the request, but is not returning any content."),
    301: ("Moved Permanently", "The requested resource has been permanently moved to a new URI."),
    302: ("Found", "The requested resource resides temporarily under a different URI."),
    304: ("Not Modified", "The resource has not been modified since the last request."),
    400: ("Bad Request", "The server cannot process the request due to invalid syntax or malformed parameters."),
    403: ("Forbidden", "Access denied. You do not have permission to view or access this resource."),
    404: ("Not Found", "The server cannot find the requested file or page URL."),
    405: ("Method Not Allowed", "The HTTP method used for this request is not allowed for this route."),
    408: ("Request Timeout", "The server timed out waiting for the client to send a complete request."),
    409: ("Conflict", "The request could not be completed due to a conflict with the current state of the resource."),
    410: ("Gone", "The requested resource is permanently deleted and no longer available on this server."),
    411: ("Length Required", "The request was rejected because Content-Length header field is required."),
    413: ("Payload Too Large", "The uploaded payload size exceeds the maximum limit configured on this server."),
    414: ("URI Too Long", "The request URI exceeds the maximum length limit allowed by the server."),
    415: ("Unsupported Media Type", "The media format of the requested data is not supported by the server."),
    500: ("Internal Server Error", "The server encountered an unexpected condition that prevented it from fulfilling the request."),
    502: ("Bad Gateway", "The server received an invalid or corrupt response from an upstream server or gateway."),
    504: ("Gateway Timeout", "The CGI script or upstream server failed to respond within the allowed timeout duration."),
    505: ("HTTP Version Not Supported", "The HTTP protocol version used in the request is not supported by this server."),
}

template = """<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>{code} {title} | WebServ</title>
    <link rel="stylesheet" href="https://cdnjs.cloudflare.com/ajax/libs/font-awesome/6.4.0/css/all.min.css">
    <style>
        :root {{
            --bg-dark: #050508;
            --bg-card: #12121a;
            --text-main: #ffffff;
            --text-muted: #8b8b9e;
            --neon-cyan: #00e5ff;
            --gradient-cyan: linear-gradient(135deg, #00e5ff 0%, #0077ff 100%);
        }}

        * {{ margin: 0; padding: 0; box-sizing: border-box; }}

        body {{
            background-color: var(--bg-dark);
            color: var(--text-main);
            font-family: 'Segoe UI', system-ui, -apple-system, sans-serif;
            display: flex;
            justify-content: center;
            align-items: center;
            min-height: 100vh;
            padding: 20px;
            overflow: hidden;
        }}

        .error-card {{
            background: var(--bg-card);
            border: 1px solid rgba(0, 229, 255, 0.25);
            padding: 60px 50px;
            border-radius: 25px;
            text-align: center;
            max-width: 580px;
            width: 100%;
            box-shadow: 0 0 50px rgba(0, 229, 255, 0.15);
            animation: fadeIn 0.5s ease-out;
        }}

        @keyframes fadeIn {{
            from {{ opacity: 0; transform: translateY(20px); }}
            to {{ opacity: 1; transform: translateY(0); }}
        }}

        .error-code {{
            font-size: 7rem;
            font-weight: 800;
            line-height: 1;
            background: var(--gradient-cyan);
            -webkit-background-clip: text;
            -webkit-text-fill-color: transparent;
            text-shadow: 0 0 40px rgba(0, 229, 255, 0.4);
            margin-bottom: 10px;
            letter-spacing: -2px;
        }}

        .divider {{
            width: 140px;
            height: 3px;
            background: linear-gradient(to right, transparent, var(--neon-cyan), transparent);
            margin: 20px auto;
        }}

        .error-message {{
            font-size: 1.8rem;
            font-weight: 700;
            color: var(--text-main);
            margin-bottom: 15px;
            text-transform: uppercase;
            letter-spacing: 1px;
        }}

        .error-sub {{
            font-size: 1.05rem;
            color: var(--text-muted);
            margin-bottom: 35px;
            line-height: 1.6;
        }}

        .btn {{
            padding: 14px 36px;
            border-radius: 50px;
            text-decoration: none;
            font-weight: 700;
            display: inline-flex;
            align-items: center;
            justify-content: center;
            gap: 10px;
            background: var(--gradient-cyan);
            color: #000;
            box-shadow: 0 4px 20px rgba(0, 229, 255, 0.3);
            transition: all 0.3s;
            text-transform: uppercase;
            letter-spacing: 1px;
            font-size: 0.95rem;
        }}

        .btn:hover {{
            transform: translateY(-3px);
            box-shadow: 0 8px 30px rgba(0, 229, 255, 0.5);
        }}
    </style>
</head>
<body>
    <div class="error-card">
        <div class="error-code">{code}</div>
        <div class="divider"></div>
        <div class="error-message">{title}</div>
        <div class="error-sub">{description}</div>
        <a href="/" class="btn"><i class="fas fa-home"></i> Return Home</a>
    </div>
</body>
</html>"""

for code, (title, description) in pages.items():
    with open(f"{code}.html", "w") as f:
        f.write(template.format(code=code, title=title, description=description))
    print(f"wrote {code}.html")