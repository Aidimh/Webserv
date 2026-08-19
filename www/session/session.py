#!/usr/bin/env python3
import os
import sys
import time
import json
import hashlib
import random
import string

# ── config ────────────────────────────────────────────────────────────────────
USERS           = {"admin": "1337", "user1": "secret42"}
SESSION_DIR     = "/tmp/webserv_sessions"
SESSION_TIMEOUT = 20         # seconds
COOKIE_NAME     = "session_id"

# ── helpers ───────────────────────────────────────────────────────────────────
def ensure_session_dir():
    if not os.path.exists(SESSION_DIR):
        os.makedirs(SESSION_DIR)

def generate_session_id():
    chars = string.ascii_letters + string.digits
    return ''.join(random.choice(chars) for _ in range(32))

def save_session(sid, username):
    ensure_session_dir()
    data = {"username": username, "created": time.time()}
    with open(os.path.join(SESSION_DIR, sid), "w") as f:
        json.dump(data, f)

def load_session(sid):
    path = os.path.join(SESSION_DIR, sid)
    if not os.path.exists(path):
        return None
    with open(path) as f:
        data = json.load(f)
    if time.time() - data["created"] > SESSION_TIMEOUT:
        os.remove(path)
        return None
    return data

def delete_session(sid):
    path = os.path.join(SESSION_DIR, sid)
    if os.path.exists(path):
        os.remove(path)

def get_cookie(name):
    raw = os.environ.get("HTTP_COOKIE", "")
    for part in raw.split(";"):
        part = part.strip()
        if part.startswith(name + "="):
            return part[len(name)+1:]
    return None

def time_left(created):
    left = SESSION_TIMEOUT - (time.time() - created)
    return max(0, int(left) + 1)  # ← +1 to round up

# ── HTML helpers ──────────────────────────────────────────────────────────────
STYLE = """
<style>
  :root {
    --bg-dark: #050508; --bg-card: #12121a; --bg-input: #1a1a24;
    --text-main: #ffffff; --text-muted: #8b8b9e;
    --neon-cyan: #00e5ff; --neon-purple: #bd00ff; --neon-pink: #ff0055; --neon-green: #39ff14;
    --gradient-cyan: linear-gradient(135deg,#00e5ff 0%,#0077ff 100%);
    --gradient-purple: linear-gradient(135deg,#bd00ff 0%,#7000ff 100%);
    --gradient-pink: linear-gradient(135deg,#ff0055 0%,#ff00aa 100%);
  }
  * { margin:0; padding:0; box-sizing:border-box; }
  body { font-family:'Segoe UI',system-ui,sans-serif; background:var(--bg-dark);
         color:var(--text-main); display:flex; align-items:center;
         justify-content:center; min-height:100vh; }
  .card { background:var(--bg-card); border:1px solid rgba(255,255,255,.07);
          border-radius:24px; padding:50px 40px; width:100%; max-width:480px;
          box-shadow:0 30px 80px rgba(0,0,0,.5); }
  h1 { font-size:2rem; font-weight:800; margin-bottom:8px;
       background:var(--gradient-cyan); -webkit-background-clip:text;
       -webkit-text-fill-color:transparent; }
  .sub { color:var(--text-muted); margin-bottom:36px; font-size:.95rem; }
  label { display:block; font-size:.8rem; font-weight:700; text-transform:uppercase;
          letter-spacing:1px; margin-bottom:8px; }
  input[type=text],input[type=password] {
    width:100%; padding:14px 18px; background:var(--bg-input);
    border:2px solid rgba(255,255,255,.1); border-radius:12px;
    color:var(--text-main); font-size:1rem; margin-bottom:22px; outline:none;
    transition:border-color .3s; }
  input:focus { border-color:var(--neon-purple);
                box-shadow:0 0 20px rgba(189,0,255,.2); }
  .btn { width:100%; padding:15px; border:none; border-radius:50px; font-size:1rem;
         font-weight:700; text-transform:uppercase; letter-spacing:1px;
         cursor:pointer; transition:all .3s; }
  .btn-purple { background:var(--gradient-purple); color:#fff;
                box-shadow:0 4px 20px rgba(189,0,255,.3); }
  .btn-purple:hover { transform:translateY(-2px); box-shadow:0 8px 30px rgba(189,0,255,.5); }
  .btn-pink { background:var(--gradient-pink); color:#fff;
              box-shadow:0 4px 20px rgba(255,0,85,.3); margin-top:14px; }
  .btn-pink:hover { transform:translateY(-2px); }
  .btn-cyan { background:var(--gradient-cyan); color:#000;
              box-shadow:0 4px 20px rgba(0,229,255,.3); }
  .btn-cyan:hover { transform:translateY(-2px); }
  .error { background:rgba(255,0,85,.12); border:1px solid rgba(255,0,85,.4);
           color:var(--neon-pink); border-radius:12px; padding:14px 18px;
           margin-bottom:22px; font-size:.9rem; }
  .badge { display:inline-flex; align-items:center; gap:8px; padding:8px 18px;
           border-radius:50px; font-size:.85rem; font-weight:700;
           text-transform:uppercase; letter-spacing:1px; }
  .badge-green { background:rgba(57,255,20,.12); color:var(--neon-green);
                 border:1px solid rgba(57,255,20,.3); }
  .badge-pink  { background:rgba(255,0,85,.12);  color:var(--neon-pink);
                 border:1px solid rgba(255,0,85,.3); }
  .dot { width:9px; height:9px; border-radius:50%; background:currentColor;
         animation:pulse 2s infinite; }
  @keyframes pulse { 0%,100%{opacity:1} 50%{opacity:.3} }
  .info-row { display:flex; justify-content:space-between; align-items:center;
              padding:14px 0; border-bottom:1px solid rgba(255,255,255,.05); }
  .info-row:last-child { border-bottom:none; }
  .info-label { color:var(--text-muted); font-size:.85rem; text-transform:uppercase;
                letter-spacing:1px; }
  .info-value { font-weight:700; color:var(--neon-cyan); }
  .timer { font-size:2.5rem; font-weight:800; color:var(--neon-cyan);
           text-align:center; margin:20px 0 4px;
           text-shadow:0 0 20px rgba(0,229,255,.4); }
  .timer-label { text-align:center; color:var(--text-muted);
                 font-size:.8rem; text-transform:uppercase; letter-spacing:2px;
                 margin-bottom:28px; }
  .back { display:block; text-align:center; margin-top:24px;
          color:var(--text-muted); text-decoration:none; font-size:.9rem; }
  .back:hover { color:var(--neon-cyan); }
</style>
"""

def page(title, body, extra_headers=""):
    print("Content-Type: text/html")
    if extra_headers:
        print(extra_headers, end="")
    print("")
    print(f"""<!DOCTYPE html>
<html lang="en">
<head><meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>{title} — WebServ Sessions</title>
{STYLE}
</head>
<body>
<div class="card">
{body}
</div>
</body></html>""")

# ── pages ─────────────────────────────────────────────────────────────────────
def show_login(error=""):
    err_html = f'<div class="error">&#9888; {error}</div>' if error else ""
    body = f"""
<h1>&#128274; Session Login</h1>
<p class="sub">Enter your credentials to start a 60-second session.</p>
{err_html}
<form method="POST" action="/session/session.py">
  <input type="hidden" name="action" value="login">
  <div>
    <label for="u">Username</label>
    <input type="text" id="u" name="username" placeholder="admin" required autofocus>
  </div>
  <div>
    <label for="p">Password</label>
    <input type="password" id="p" name="password" placeholder="••••••••" required>
  </div>
  <button type="submit" class="btn btn-purple">&#128275; Sign In</button>
</form>
<a href="/" class="back">&#8592; Back to WebServ</a>
"""
    page("Login", body)

def show_session(session_data, sid):
    username = session_data["username"]
    left     = time_left(session_data["created"])
    body = f"""
<h1>&#9989; Session Active</h1>
<p class="sub">You are authenticated. Session expires automatically.</p>

<div style="margin:28px 0 8px">
  <span class="badge badge-green"><span class="dot"></span> Connected</span>
</div>

<div class="timer" id="countdown">{left}</div>
<div class="timer-label">seconds remaining</div>

<div style="margin-bottom:28px">
  <div class="info-row">
    <span class="info-label">User</span>
    <span class="info-value">{username}</span>
  </div>
  <div class="info-row">
    <span class="info-label">Session ID</span>
    <span class="info-value" style="font-size:.75rem;color:var(--text-muted)">{sid[:16]}…</span>
  </div>
  <div class="info-row">
    <span class="info-label">Timeout</span>
    <span class="info-value">60 seconds</span>
  </div>
</div>

<form method="POST" action="/session/session.py">
  <input type="hidden" name="action" value="logout">
  <button type="submit" class="btn btn-pink">&#128275; Logout</button>
</form>
<a href="/" class="back">&#8592; Back to WebServ</a>

<script>
  var left = {left};
  var t = document.getElementById('countdown');
  var iv = setInterval(function(){{
    left--;
    if (left <= 0) {{ clearInterval(iv); location.reload(); return; }}
    t.textContent = left;
    if (left <= 10) t.style.color = '#ff0055';
  }}, 1000);
</script>
"""
    page("Session Active", body)

def show_expired():
    body = """
<h1>&#9201; Session Expired</h1>
<p class="sub">Your 60-second session has ended.</p>
<div style="margin:24px 0">
  <span class="badge badge-pink"><span class="dot"></span> Disconnected</span>
</div>
<p style="color:var(--text-muted);margin:20px 0 30px">
  Please log in again to start a new session.
</p>
<a href="/session/session.py">
  <button class="btn btn-cyan">&#128275; Login Again</button>
</a>
<a href="/" class="back">&#8592; Back to WebServ</a>
"""
    page("Session Expired", body)

def show_logout():
    body = """
<h1>&#128274; Logged Out</h1>
<p class="sub">Your session has been terminated successfully.</p>
<div style="margin:24px 0">
  <span class="badge badge-pink"><span class="dot"></span> Disconnected</span>
</div>
<p style="color:var(--text-muted);margin:20px 0 30px">
  See you next time.
</p>
<a href="/session/session.py">
  <button class="btn btn-cyan" style="margin-bottom:0">&#128275; Login Again</button>
</a>
<a href="/" class="back">&#8592; Back to WebServ</a>
"""
    page("Logged Out", body)

# ── request parsing ───────────────────────────────────────────────────────────
def parse_body():
    try:
        length = int(os.environ.get("CONTENT_LENGTH", 0))
        raw    = sys.stdin.read(length) if length > 0 else ""
    except Exception:
        raw = ""
    params = {}
    for pair in raw.split("&"):
        if "=" in pair:
            k, v = pair.split("=", 1)
            params[k] = v.replace("+", " ").replace("%40", "@")
    return params

# ── main ──────────────────────────────────────────────────────────────────────
def main():
    method = os.environ.get("REQUEST_METHOD", "GET").upper()
    sid    = get_cookie(COOKIE_NAME)

    # ── POST ──────────────────────────────────────────────────────────────────
    if method == "POST":
        body   = parse_body()
        action = body.get("action", "")

        if action == "logout":
            if sid:
                delete_session(sid)
            print(f"Set-Cookie: {COOKIE_NAME}=deleted; Max-Age=0; Path=/\r")
            show_logout()
            return

        if action == "login":
            username = body.get("username", "").strip()
            password = body.get("password", "").strip()
            if username in USERS and USERS[username] == password:
                new_sid = generate_session_id()
                save_session(new_sid, username)
                print("Content-Type: text/html")
                print(f"Set-Cookie: {COOKIE_NAME}={new_sid}; Max-Age={SESSION_TIMEOUT}; Path=/")
                print("Location: /session/session.py")
                print("Status: 302 Found")
                print("")
                return

    # ── GET ───────────────────────────────────────────────────────────────────
    if sid:
        session_data = load_session(sid)
        if session_data:
            show_session(session_data, sid)
        else:
            show_expired()
    else:
        show_login()

def _session_body(session_data, sid):
    username = session_data["username"]
    left     = time_left(session_data["created"])
    return f"""
<h1>&#9989; Session Active</h1>
<p class="sub">You are authenticated. Session expires automatically.</p>
<div style="margin:28px 0 8px">
  <span class="badge badge-green"><span class="dot"></span> Connected</span>
</div>
<div class="timer" id="countdown">{left}</div>
<div class="timer-label">seconds remaining</div>
<div style="margin-bottom:28px">
  <div class="info-row">
    <span class="info-label">User</span>
    <span class="info-value">{username}</span>
  </div>
  <div class="info-row">
    <span class="info-label">Session ID</span>
    <span class="info-value" style="font-size:.75rem;color:var(--text-muted)">{sid[:16]}…</span>
  </div>
  <div class="info-row">
    <span class="info-label">Timeout</span>
    <span class="info-value">60 seconds</span>
  </div>
</div>
<form method="POST" action="/session/session.py">
  <input type="hidden" name="action" value="logout">
  <button type="submit" class="btn btn-pink">&#128275; Logout</button>
</form>
<a href="/" class="back">&#8592; Back to WebServ</a>
<script>
  var left = {left};
  var t = document.getElementById('countdown');
  var iv = setInterval(function(){{
    left--;
    if (left <= 0) {{ clearInterval(iv); location.reload(); return; }}
    t.textContent = left;
    if (left <= 10) t.style.color = '#ff0055';
  }}, 1000);
</script>"""

if __name__ == "__main__":
    main()