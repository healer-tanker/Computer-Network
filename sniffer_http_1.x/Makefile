CC     = gcc
CFLAGS = -Wall -Wextra -O2

SRC = main.c http1.c

# make : HTTP/1.x 분석기 (포트 80/8080/8000 고정)
http_sniffer: $(SRC) http1.h
	$(CC) $(CFLAGS) -o $@ $(SRC)

clean:
	rm -f http_sniffer

.PHONY: clean
