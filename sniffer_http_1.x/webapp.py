#!/usr/bin/env python3
"""
webapp.py - 패킷 분석 테스트용 미니 웹 서비스 (평문 HTTP/1.1, 표준 라이브러리만 사용)

실행:  python3 webapp.py            -> http://127.0.0.1:8000
       python3 webapp.py 0.0.0.0    -> 외부(Windows 브라우저 등)에서도 접속 가능

페이지
  GET  /                  메인 (로그인 폼 + 검색 폼)
  POST /login             로그인 처리 (form: id, pw)  -> 쿠키 발급
  GET  /search?q=...      검색 결과 페이지
  GET  /api/products      JSON API (앱이 호출하는 형태)
  GET  /mypage            로그인 쿠키 확인
"""
import json
import sys
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from urllib.parse import urlparse, parse_qs

PRODUCTS = [
    {"id": 1, "name": "network cable", "price": 5000},
    {"id": 2, "name": "router", "price": 59000},
    {"id": 3, "name": "switch hub", "price": 32000},
]
USERS = {"student": "1234"}
SESSIONS = {}

PAGE = """<!DOCTYPE html>
<html><head><meta charset="utf-8"><title>Net3 Shop</title></head>
<body>
<h1>Net3 Shop</h1>
{msg}
<h2>Login</h2>
<form method="POST" action="/login">
  ID <input name="id"> PW <input name="pw" type="password">
  <button>login</button>
</form>
<h2>Search</h2>
<form method="GET" action="/search">
  <input name="q" placeholder="router"> <button>search</button>
</form>
<p><a href="/api/products">/api/products (JSON)</a> | <a href="/mypage">mypage</a></p>
</body></html>"""


class Handler(BaseHTTPRequestHandler):
    protocol_version = "HTTP/1.1"          # 응답도 HTTP/1.1 (keep-alive)
    server_version = "Net3Shop/1.0"

    def send_body(self, code, body, ctype="text/html; charset=utf-8", headers=None):
        data = body.encode("utf-8")
        self.send_response(code)
        self.send_header("Content-Type", ctype)
        self.send_header("Content-Length", str(len(data)))
        for k, v in (headers or {}).items():
            self.send_header(k, v)
        self.end_headers()
        self.wfile.write(data)

    def user(self):
        cookie = self.headers.get("Cookie", "")
        for part in cookie.split(";"):
            k, _, v = part.strip().partition("=")
            if k == "session" and v in SESSIONS:
                return SESSIONS[v]
        return None

    def do_GET(self):
        url = urlparse(self.path)
        if url.path == "/":
            self.send_body(200, PAGE.format(msg=""))
        elif url.path == "/search":
            q = parse_qs(url.query).get("q", [""])[0]
            hits = [p for p in PRODUCTS if q.lower() in p["name"]]
            items = "".join(f"<li>{p['name']} - {p['price']}won</li>" for p in hits) or "<li>no result</li>"
            self.send_body(200, f"<h1>search: {q}</h1><ul>{items}</ul><a href='/'>home</a>")
        elif url.path == "/api/products":
            self.send_body(200, json.dumps(PRODUCTS), "application/json")
        elif url.path == "/mypage":
            u = self.user()
            if u:
                self.send_body(200, f"<h1>hello, {u}</h1>")
            else:
                self.send_body(401, "<h1>401 login required</h1>")
        else:
            self.send_body(404, "<h1>404 not found</h1>")

    def do_POST(self):
        url = urlparse(self.path)
        length = int(self.headers.get("Content-Length", 0))
        form = parse_qs(self.rfile.read(length).decode("utf-8"))
        if url.path == "/login":
            uid = form.get("id", [""])[0]
            pw = form.get("pw", [""])[0]
            if USERS.get(uid) == pw:
                token = f"tok{len(SESSIONS) + 1:04d}"
                SESSIONS[token] = uid
                self.send_body(302, "", headers={"Location": "/mypage",
                                                 "Set-Cookie": f"session={token}; Path=/"})
            else:
                self.send_body(200, PAGE.format(msg="<p style='color:red'>login failed</p>"))
        else:
            self.send_body(404, "<h1>404 not found</h1>")


if __name__ == "__main__":
    host = sys.argv[1] if len(sys.argv) > 1 else "127.0.0.1"
    print(f"Net3 Shop running on http://{host}:8000  (Ctrl+C to stop)")
    ThreadingHTTPServer((host, 8000), Handler).serve_forever()
