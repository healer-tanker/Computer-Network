/* http1.c - HTTP/1.x 파서 (RFC 9112)
 * 메시지 = 시작줄 CRLF + 헤더 줄들 CRLF + 빈 줄 CRLF + [본문] */
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "http1.h"

int looks_like_http1(const unsigned char *p, int len)
{
    static const char *starts[] = {
        "GET ", "POST ", "PUT ", "DELETE ", "HEAD ", "OPTIONS ",
        "PATCH ", "CONNECT ", "TRACE ", "HTTP/1.", NULL
    };
    for (int i = 0; starts[i]; i++) {
        int n = (int)strlen(starts[i]);
        if (len >= n && memcmp(p, starts[i], n) == 0) return 1;
    }
    return 0;
}

/* '\n' 까지의 길이, 없으면 -1 */
static int line_len(const unsigned char *p, int len)
{
    for (int i = 0; i < len; i++) if (p[i] == '\n') return i;
    return -1;
}

/* 끝의 '\r' 제거, 출력 불가 문자는 '.' 로 */
static void print_line(const char *prefix, const unsigned char *p, int n)
{
    if (n > 0 && p[n - 1] == '\r') n--;
    printf("%s", prefix);
    for (int i = 0; i < n; i++) putchar(isprint(p[i]) ? p[i] : '.');
    putchar('\n');
}

void parse_http1(const unsigned char *p, int len)
{
    /* 1) 시작줄 */
    if (len <= 0) return;
    int n = line_len(p, len);
    if (n < 0) n = len;
    char start[1024];
    int sn = n < (int)sizeof(start) - 1 ? n : (int)sizeof(start) - 1;
    if (sn < 0) sn = 0;
    memcpy(start, p, sn);
    start[sn] = '\0';
    if (sn > 0 && start[sn - 1] == '\r') start[--sn] = '\0';

    if (strncmp(start, "HTTP/", 5) == 0) {          /* 응답: Version Status Reason */
        char ver[16] = "", reason[256] = "";
        int code = 0;
        sscanf(start, "%15s %d %255[^\r\n]", ver, &code, reason);
        printf("  [HTTP/1.x Response]\n");
        printf("    Version     : %s\n", ver);
        printf("    Status Code : %d\n", code);
        printf("    Reason      : %s\n", reason);
    } else {                                         /* 요청: Method URI Version */
        char method[16] = "", uri[768] = "", ver[16] = "";
        sscanf(start, "%15s %767s %15s", method, uri, ver);
        printf("  [HTTP/1.x Request]\n");
        printf("    Method      : %s\n", method);
        printf("    URI         : %s\n", uri);
        printf("    Version     : %s\n", ver[0] ? ver : "(none -> HTTP/0.9)");
    }

    /* 2) 헤더: 빈 줄이 나올 때까지 */
    int off = n + 1;
    printf("    Headers     :\n");
    while (off < len) {
        int ln = line_len(p + off, len - off);
        if (ln < 0) { print_line("      (truncated) ", p + off, len - off); return; }
        if (ln == 0 || (ln == 1 && p[off] == '\r')) {      /* 헤더 끝 */
            off += ln + 1;
            if (off < len) {                              /* 3) 본문 미리보기 */
                int blen = len - off;
                printf("    Body        : %d bytes (this segment)\n", blen);
                print_line("      preview : ", p + off, blen < 200 ? blen : 200);
            }
            return;
        }
        print_line("      ", p + off, ln);
        off += ln + 1;
    }
}
