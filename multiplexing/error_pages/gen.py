pages = {
    200: "OK",
    201: "Created",
    204: "No Content",
    301: "Moved Permanently",
    302: "Found",
    304: "Not Modified",
    400: "Bad Request",
    403: "Forbidden",
    404: "Not Found",
    405: "Method Not Allowed",
    408: "Request Timeout",
    409: "Conflict",
    410: "Gone",
    411: "Length Required",
    413: "Payload Too Large",
    414: "URI Too Long",
    415: "Unsupported Media Type",
    500: "Internal Server Error",
    502: "Bad Gateway",
    504: "Gateway Timeout",
    505: "HTTP Version Not Supported",
}

template = """<!DOCTYPE html>
<html lang='en'>
<head>
    <meta charset='UTF-8'>
    <title>{code} {message}</title>
    <style>
        * {{ margin: 0; padding: 0; box-sizing: border-box; }}
        body {{
            background-color: #0a0a0a;
            display: flex;
            justify-content: center;
            align-items: center;
            height: 100vh;
            font-family: 'Courier New', monospace;
        }}
        .container {{
            text-align: center;
            border: 2px solid #00ff88;
            padding: 60px 80px;
            border-radius: 12px;
            box-shadow: 0 0 40px #00ff8855, 0 0 80px #00ff8822;
            background-color: #0d1f0d;
        }}
        .code {{
            font-size: 120px;
            font-weight: bold;
            color: #00ff88;
            text-shadow: 0 0 20px #00ff88, 0 0 40px #00ff88;
            letter-spacing: 10px;
        }}
        .divider {{
            width: 100%;
            height: 2px;
            background: linear-gradient(to right, transparent, #00ff88, transparent);
            margin: 20px 0;
        }}
        .message {{
            font-size: 28px;
            color: #00cc66;
            letter-spacing: 4px;
            text-transform: uppercase;
            margin-bottom: 30px;
        }}
        .sub {{
            font-size: 14px;
            color: #005522;
            letter-spacing: 2px;
        }}
        a {{
            display: inline-block;
            margin-top: 40px;
            padding: 12px 30px;
            border: 1px solid #00ff88;
            color: #00ff88;
            text-decoration: none;
            letter-spacing: 3px;
            font-size: 13px;
        }}
        a:hover {{
            background-color: #00ff88;
            color: #0a0a0a;
        }}
    </style>
</head>
<body>
    <div class='container'>
        <div class='code'>{code}</div>
        <div class='divider'></div>
        <div class='message'>{message}</div>
        <div class='sub'>the server could not process your request</div>
        <a href='/'>return home</a>
    </div>
</body>
</html>"""

for code, message in pages.items():
    with open(f"{code}.html", "w") as f:
        f.write(template.format(code=code, message=message))
    print(f"wrote {code}.html")