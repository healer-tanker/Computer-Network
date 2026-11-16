// h2server.js - HTTP/2 평문(h2c) 테스트 서버 (port 8000)
const http2 = require('http2');
http2.createServer().on('stream', (s, h) => {
  s.respond({':status': 200, 'content-type': 'text/plain'});
  s.end('hello h2 ' + h[':path']);
}).listen(8000, '127.0.0.1');
console.log('h2c server on http://127.0.0.1:8000');
