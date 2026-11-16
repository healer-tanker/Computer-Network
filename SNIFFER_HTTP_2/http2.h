/* http2.h - HTTP/2 (h2c) 프레임 파서 (실습용: 프레임 구조만, HPACK 해석 X) */
#ifndef HTTP2_H
#define HTTP2_H
#include <stdint.h>

#define H2_BUF 65536   /* 기본 최대 프레임 16KB(RFC 9113 4.2)보다 넉넉하게 */

/* 한 방향(C->S 또는 S->C)의 상태: 세그먼트에 걸친 프레임을 모으는 버퍼 */
typedef struct {
    uint8_t buf[H2_BUF];
    int     len;
    int     preface_done;   /* 클라이언트 방향: 24바이트 프리페이스 처리 여부 */
} h2_dir_t;

/* TCP 페이로드를 넣으면 완성된 프레임을 꺼내 출력. dirn: 0=C->S, 1=S->C */
void h2_feed(h2_dir_t *d, const uint8_t *data, int len, int dirn);
/* 버퍼 비우기 (TCP 누락 시) */
void h2_reset(h2_dir_t *d);

#endif
