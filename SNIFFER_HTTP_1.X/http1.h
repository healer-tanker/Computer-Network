/* http1.h - HTTP/1.x 텍스트 파서 */
#ifndef HTTP1_H
#define HTTP1_H

/* payload 가 HTTP/1.x 요청/응답 시작줄로 시작하면 1 */
int  looks_like_http1(const unsigned char *p, int len);
/* 시작줄 + 헤더 + 본문 미리보기 출력 */
void parse_http1(const unsigned char *p, int len);

#endif
