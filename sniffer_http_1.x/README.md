# HTTP/2 패킷 분석기 (Raw Socket, 실습용)

TCP 포트 80 / 8080 / 8000 의 트래픽만 잡고, 평문 HTTP/2(h2c) 로 고정해서 프레임 구조를 출력한다.
(버전 판별 없음, HTTP/1.x 미지원, HPACK 헤더 해석 안 함 - 헤더 블록은 크기만 출력)

## 파일 구성
| 파일 | 역할 |
|---|---|
| main.c | Raw Socket, Ethernet/IP/TCP 파싱, 포트 필터(80/8080/8000), TCP 순서 추적 |
| http2.c/h | HTTP/2 프리페이스, 9바이트 프레임 헤더, 프레임 타입별 출력 |
| h2server.js | 테스트용 평문 HTTP/2 서버 (포트 8000, Node.js) |

## 빌드
    make        # -> ./http_sniffer

## 실행
    sudo ./http_sniffer lo                                       # 터미널 1
    node h2server.js                                             # 터미널 2
    curl --http2-prior-knowledge http://127.0.0.1:8000/hello     # 터미널 3
    curl --http2-prior-knowledge -d "id=student&pw=1234" http://127.0.0.1:8000/login
