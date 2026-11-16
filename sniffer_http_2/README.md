# HTTP/1.x 패킷 분석기 (Raw Socket)

TCP 포트 80 / 8080 / 8000 의 트래픽만 잡고, HTTP/1.x 로 고정해서 해석한다.
(버전 판별 없음, HTTP/2 미지원)

## 파일 구성
| 파일 | 역할 |
|---|---|
| main.c | Raw Socket, Ethernet/IP/TCP 파싱, 포트 필터(80/8080/8000), TCP 순서 추적 |
| http1.c/h | HTTP/1.x 시작줄 + 헤더 + 본문 미리보기 |
| webapp.py | 테스트용 평문 HTTP/1.1 미니 웹서비스 (포트 8000) |

## 빌드
    make        # -> ./http_sniffer

## 실행
    sudo ./http_sniffer lo               # 터미널 1
    python3 webapp.py                    # 터미널 2
    curl -d "id=student&pw=1234" http://127.0.0.1:8000/login   # 터미널 3
