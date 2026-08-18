#!/usr/bin/env python3
import cgi
import os
import time
import json
import secrets

# --- CONFIGURATION ---
SESSION_FILE = "/tmp/webserv_sessions.json"
SESSION_TIMEOUT = 60  # 1 minute
USERS = {
    "admin": "password123",
    "user1": "secret"
}

# --- HELPER FUNCTIONS ---
def load_sessions():
    try:
        with open(SESSION_FILE, 'r') as f:
            return json.load(f)
    except (FileNotFoundError, json.JSONDecodeError):
        return {}

def save_sessions(sessions):
    with open(SESSION_FILE, 'w') as f:
        json.dump(sessions, f)

def get_cookie_value(cookie_name):
    cookie_string = os.environ.get('HTTP_COOKIE', '')
    for cookie in cookie_string.split(';'):
        if '=' in cookie:
            name, value = cookie.strip().split('=', 1)
            if name == cookie_name:
                return value
    return None

# --- MAIN LOGIC ---
def main():
    form = cgi.FieldStorage()
    sessions = load_sessions()
    current_time = time.time()
    
    # 1. Handle Login Submission (POST)
    if os.environ.get('REQUEST_METHOD') == 'POST':
        username = form.getvalue('username', '')
        password = form.getvalue('password', '')
        
        if username in USERS and USERS[username] == password:
            # Create new secure session
            session_id = secrets.token_hex(16)
            sessions[session_id] = {
                "username": username,
                "created_at": current_time
            }
            save_sessions(sessions)
            
            # Send cookie and redirect to welcome
            print("Status: 302 Found")
            print(f"Set-Cookie: session_id={session_id}; Path=/; Max-Age=60")
            print("Location: /cgi-bin/session.py") # Redirect to self to show welcome
            print()
            return
        else:
            # Failed login: show form with error
            print("Content-Type: text/html\n")
            print_html_form(error="Invalid username or password!")
            return

    # 2. Check for Existing Valid Session (GET)
    cookie_id = get_cookie_value('session_id')
    
    if cookie_id and cookie_id in sessions:
        session_data = sessions[cookie_id]
        # Check if expired
        if (current_time - session_data["created_at"]) < SESSION_TIMEOUT:
            # VALID: Show welcome page
            print("Content-Type: text/html\n")
            print_html_welcome(session_data["username"])
            return
        else:
            # EXPIRED: Clean up and show login
            del sessions[cookie_id]
            save_sessions(sessions)
            print("Content-Type: text/html")
            print("Set-Cookie: session_id=; Path=/; Max-Age=0") # Clear cookie
            print()
            print_html_form(error="Session expired. Please log in again.")
            return

    # 3. No valid session: Show login form
    print("Content-Type: text/html\n")
    print_html_form()

# --- HTML OUTPUT FUNCTIONS ---
def print_html_form(error=None):
    error_html = f'<p style="color: #ff3366; text-align: center;">{error}</p>' if error else ''
    print(f"""<!DOCTYPE html>
    <html>
    <head>
        <title>Login</title>
        <style>
            body {{ background: #0a0a0a; color: #cc66ff; font-family: monospace; display: flex; justify-content: center; align-items: center; height: 100vh; margin: 0; }}
            .box {{ border: 1px solid #cc66ff; padding: 30px; border-radius: 8px; text-align: center; background: #0d1f0d; box-shadow: 0 0 20px rgba(204, 102, 255, 0.2); width: 300px; }}
            input {{ width: 90%; padding: 10px; margin: 10px 0; background: #000; border: 1px solid #cc66ff; color: #cc66ff; font-family: monospace; }}
            button {{ width: 95%; padding: 10px; background: transparent; border: 1px solid #cc66ff; color: #cc66ff; cursor: pointer; font-family: monospace; font-weight: bold; }}
            button:hover {{ background: rgba(204, 102, 255, 0.2); }}
        </style>
    </head>
    <body>
        <div class="box">
            <h2>LOGIN</h2>
            {error_html}
            <form method="POST">
                <input type="text" name="username" placeholder="Username (admin)" required>
                <input type="password" name="password" placeholder="Password (password123)" required>
                <button type="submit">LOGIN</button>
            </form>
        </div>
    </body>
    </html>""")

def print_html_welcome(username):
    print(f"""<!DOCTYPE html>
    <html>
    <head>
        <title>Welcome</title>
        <style>
            body {{ background: #0a0a0a; color: #00ff88; font-family: monospace; display: flex; justify-content: center; align-items: center; height: 100vh; margin: 0; }}
            .box {{ border: 1px solid #00ff88; padding: 30px; border-radius: 8px; text-align: center; background: #0d1f0d; box-shadow: 0 0 20px rgba(0, 255, 136, 0.2); }}
            a {{ color: #ff3366; text-decoration: none; border: 1px solid #ff3366; padding: 10px 20px; display: inline-block; margin-top: 20px; }}
            a:hover {{ background: rgba(255, 51, 102, 0.2); }}
        </style>
    </head>
    <body>
        <div class="box">
            <h2>WELCOME, {username}!</h2>
            <p>Your session is active.</p>
            <p>Refresh this page. If you wait 60 seconds, it will expire.</p>
            <a href="/cgi-bin/session.py?logout=1">LOGOUT</a>
        </div>
    </body>
    </html>""")

# Handle Logout
if os.environ.get('QUERY_STRING', '') == 'logout=1':
    cookie_id = get_cookie_value('session_id')
    if cookie_id:
        sessions = load_sessions()
        if cookie_id in sessions:
            del sessions[cookie_id]
            save_sessions(sessions)
    print("Status: 302 Found")
    print("Set-Cookie: session_id=; Path=/; Max-Age=0")
    print("Location: /cgi-bin/session.py")
    print()
else:
    main()