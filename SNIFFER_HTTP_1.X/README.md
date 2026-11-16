# HTTP/1.x 패킷 분석기 (Raw Socket)

TCP 포트 80 / 8080 / 8000 의 트래픽만 잡고, HTTP/1.x 로 고정해서 해석한다.
(버전 판별 없음, HTTP/2 미지원)

## 파일 구성
| 파일 | 역할 |
|---|---|
| main.c | Raw Socket, Ethernet/IP/TCP 파싱, 포트 필터(80/8080/8000), TCP 순서 추적 |
| http1.c/h | HTTP/1.x 시작줄 + 헤더 + 본문 미리보기 |
| webapp.py | 테스트용 평문 HTTP/1.1 미니 웹서비스 (포트 8000) |

## 준비 (처음 한 번)
```bash
sudo apt install build-essential curl python3
```

## Makefile 타겟
| 명령 | 동작 |
|---|---|
| `make` | `gcc -Wall -Wextra -O2 -o http1_sniffer main.c http1.c` |
| `make clean` | `http1_sniffer` 삭제 |

## 실행 방법 (터미널 3개)
```bash
# 터미널 1: 빌드 + 스니퍼 실행 (raw socket 이라 sudo 필요)
cd SNIFFER_HTTP_1.X
make
sudo ./http1_sniffer lo

# 터미널 2: 테스트 웹서버 (포트 8000)
cd SNIFFER_HTTP_1.X
python3 webapp.py

# 터미널 3: 요청 보내기
curl http://127.0.0.1:8000/
curl -d "id=student&pw=1234" http://127.0.0.1:8000/login
curl "http://127.0.0.1:8000/search?q=router"

# 종료: 터미널 1, 2 에서 Ctrl+C 후
make clean
```

## 주의
- sudo 없이 실행하면 `socket (root 권한/CAP_NET_RAW 필요)` 에러가 난다.
- 포트 8000 을 쓰는 다른 서버(h2server.js 등)와 동시에 띄우지 않는다.
