/* http2.c - HTTP/2 프레임 파서 (RFC 9113)
 * 프레임 = 9바이트 헤더 [길이 24bit | 타입 8bit | 플래그 8bit | R 1bit + 스트림ID 31bit] + 페이로드
 * 실습 범위: 프레임 구조 출력. HEADERS 의 헤더 블록은 HPACK 압축이라 크기만 출력한다. */
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "http2.h"

#define PREFACE_LEN 24
static const char PREFACE[] = "PRI * HTTP/2.0\r\n\r\nSM\r\n\r\n";

static uint32_t be24(const uint8_t *p) { return (p[0] << 16) | (p[1] << 8) | p[2]; }
static uint32_t be32(const uint8_t *p) { return ((uint32_t)p[0] << 24) | (p[1] << 16) | (p[2] << 8) | p[3]; }

static const char *type_name(uint8_t t)
{
    static const char *names[] = { "DATA", "HEADERS", "PRIORITY", "RST_STREAM", "SETTINGS",
                                   "PUSH_PROMISE", "PING", "GOAWAY", "WINDOW_UPDATE", "CONTINUATION" };
    return t < 10 ? names[t] : "UNKNOWN";
}

/* 프레임 하나 출력 */
static void print_frame(const uint8_t *f, int dirn)
{
    uint32_t len   = be24(f);
    uint8_t  type  = f[3], flags = f[4];
    uint32_t sid   = be32(f + 5) & 0x7fffffff;   /* 최상위 1비트(R)는 예약 */
    const uint8_t *pl = f + 9;

    printf("  [HTTP/2 Frame] %s  %-13s len=%-5u stream=%u flags=0x%02x",
           dirn == 0 ? "C->S" : "S->C", type_name(type), len, sid, flags);
    if ((type == 0 || type == 1) && (flags & 0x1)) printf(" END_STREAM");
    if ((type == 1 || type == 9) && (flags & 0x4)) printf(" END_HEADERS");
    if ((type == 4 || type == 6) && (flags & 0x1)) printf(" ACK");
    printf("\n");

    switch (type) {
    case 0: {                                   /* DATA: 본문 (압축 안 됨) */
        int off = 0, pad = 0;
        if (flags & 0x8) { pad = pl[0]; off = 1; }   /* PADDED */
        int n = (int)len - off - pad;
        printf("      data %d bytes: \"", n > 0 ? n : 0);
        for (int i = 0; i < n && i < 80; i++) putchar(isprint(pl[off + i]) ? pl[off + i] : '.');
        printf("%s\"\n", n > 80 ? "..." : "");
        break;
    }
    case 1:                                     /* HEADERS */
    case 9:                                     /* CONTINUATION */
        printf("      header block %u bytes (HPACK compressed, not decoded)\n", len);
        break;
    case 3:                                     /* RST_STREAM: 에러코드 4B */
        if (len >= 4) printf("      error code=%u\n", be32(pl));
        break;
    case 4:                                     /* SETTINGS: (ID 2B + 값 4B) 반복 */
        for (uint32_t i = 0; i + 6 <= len; i += 6)
            printf("      setting id=%u value=%u\n", (pl[i] << 8) | pl[i + 1], be32(pl + i + 2));
        break;
    case 7:                                     /* GOAWAY: 마지막 스트림 4B + 에러코드 4B */
        if (len >= 8) printf("      last stream=%u error code=%u\n", be32(pl) & 0x7fffffff, be32(pl + 4));
        break;
    case 8:                                     /* WINDOW_UPDATE: 증가량 4B */
        if (len >= 4) printf("      increment=%u\n", be32(pl) & 0x7fffffff);
        break;
    }
}

void h2_feed(h2_dir_t *d, const uint8_t *data, int len, int dirn)
{
    /* 1) 버퍼에 이어 붙이기 (프레임이 TCP 세그먼트 여러 개에 걸칠 수 있음) */
    if (d->len + len > H2_BUF) { printf("  [!] buffer overflow, reset\n"); d->len = 0; }
    if (len > H2_BUF) return;
    memcpy(d->buf + d->len, data, len);
    d->len += len;

    /* 2) 클라이언트 첫 24바이트 = 연결 프리페이스 */
    int off = 0;
    if (dirn == 0 && !d->preface_done) {
        if (d->len < PREFACE_LEN) return;
        if (memcmp(d->buf, PREFACE, PREFACE_LEN) == 0)
            printf("  [HTTP/2] client connection preface\n");
        off = PREFACE_LEN;
        d->preface_done = 1;
    }

    /* 3) 9바이트 헤더의 길이 필드로 프레임 단위로 자르기 */
    while (d->len - off >= 9) {
        int flen = (int)be24(d->buf + off);
        if (d->len - off < 9 + flen) break;      /* 아직 덜 옴 -> 다음 세그먼트 대기 */
        print_frame(d->buf + off, dirn);
        off += 9 + flen;
    }

    /* 4) 남은(미완성) 바이트를 앞으로 당김 */
    memmove(d->buf, d->buf + off, d->len - off);
    d->len -= off;
}

void h2_reset(h2_dir_t *d) { d->len = 0; }
