CC     = gcc
CFLAGS = -Wall -Wextra -O2

SRC = main.c http2.c

# make : HTTP/2(h2c) 프레임 분석기 (포트 80/8080/8000 고정)
http2_sniffer: $(SRC) http2.h
	$(CC) $(CFLAGS) -o $@ $(SRC)

clean:
	rm -f http2_sniffer

.PHONY: clean
