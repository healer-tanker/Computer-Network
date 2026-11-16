# HTTP/2 패킷 분석기 (Raw Socket, 실습용)

TCP 포트 80 / 8080 / 8000 의 트래픽만 잡고, 평문 HTTP/2(h2c) 로 고정해서 프레임 구조를 출력한다.
(버전 판별 없음, HTTP/1.x 미지원, HPACK 헤더 해석 안 함 - 헤더 블록은 크기만 출력)

## 파일 구성
| 파일 | 역할 |
|---|---|
| main.c | Raw Socket, Ethernet/IP/TCP 파싱, 포트 필터(80/8080/8000), TCP 순서 추적 |
| http2.c/h | HTTP/2 프리페이스, 9바이트 프레임 헤더, 프레임 타입별 출력 |
| h2server.js | 테스트용 평문 HTTP/2 서버 (포트 8000, Node.js) |

## 준비 (처음 한 번)
```bash
sudo apt install build-essential curl nodejs
```

## Makefile 타겟
| 명령 | 동작 |
|---|---|
| `make` | `gcc -Wall -Wextra -O2 -o http_sniffer main.c http2.c` |
| `make clean` | `http_sniffer` 삭제 |

## 실행 방법 (터미널 3개)
```bash
# 터미널 1: 빌드 + 스니퍼 실행 (raw socket 이라 sudo 필요)
cd sniffer_rev2
make
sudo ./http_sniffer lo

# 터미널 2: 테스트 h2c 서버 (포트 8000)
cd sniffer_rev2
node h2server.js

# 터미널 3: 요청 보내기 (--http2-prior-knowledge 를 붙여야 HTTP/2 로 감)
curl --http2-prior-knowledge http://127.0.0.1:8000/hello
curl --http2-prior-knowledge -d "id=student&pw=1234" http://127.0.0.1:8000/login

# 종료: 터미널 1, 2 에서 Ctrl+C 후
make clean
```

## 주의
- sudo 없이 실행하면 `socket (root 권한/CAP_NET_RAW 필요)` 에러가 난다.
- 포트 8000 을 쓰는 다른 서버(webapp.py 등)와 동시에 띄우지 않는다.
- curl 에 HTTP/2 지원이 있는지 `curl -V` 출력의 Features 에 `HTTP2` 가 있는지로 확인한다.
