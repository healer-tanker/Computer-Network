/*
 * main.c - Raw Socket(AF_PACKET, SOCK_RAW) 기반 HTTP/2 (h2c) 패킷 분석기
 *
 *   Ethernet -> IPv4 -> TCP(포트 80/8080/8000 고정) -> (TCP 순서 정렬) -> HTTP/2 프레임 출력
 *   버전 판별 없음: 위 포트의 TCP 데이터는 평문 HTTP/2 로 간주한다.
 *
 * 빌드: make
 * 실행: sudo ./http_sniffer [인터페이스|any]
 *       예) sudo ./http_sniffer lo
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <time.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <net/if.h>
#include <linux/if_packet.h>
#include <net/ethernet.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include "http2.h"

#define BUF_SIZE 65536
#define MAX_CONN 64

/* TCP 연결 하나 = 방향 2개 (0: client->server, 1: server->client) */
typedef struct {
    int      init;
    uint32_t next_seq;     /* 다음에 와야 할 seq -> 중복/재전송 제거 */
    h2_dir_t h2;           /* 프레임 재조립 버퍼 */
} dir_t;

typedef struct {
    int      used;
    uint32_t cip, sip;     /* client / server IP */
    uint16_t cport, sport;
    dir_t    d[2];
    time_t   last;
} conn_t;

static conn_t conns[MAX_CONN];
static volatile sig_atomic_t running = 1;

static void on_sigint(int sig) { (void)sig; running = 0; }

/* HTTP 포트 고정: 80, 8080, 8000 */
static int is_http_port(uint16_t p)
{
    return p == 80 || p == 8080 || p == 8000;
}

/* ---------------- 연결 추적 ---------------- */

static void conn_free(conn_t *c)
{
    memset(c, 0, sizeof(*c));
}

static conn_t *conn_find(uint32_t sip, uint16_t sp, uint32_t dip, uint16_t dp, int *dir)
{
    for (int i = 0; i < MAX_CONN; i++) {
        conn_t *c = &conns[i];
        if (!c->used) continue;
        if (c->cip == sip && c->cport == sp && c->sip == dip && c->sport == dp) { *dir = 0; return c; }
        if (c->cip == dip && c->cport == dp && c->sip == sip && c->sport == sp) { *dir = 1; return c; }
    }
    return NULL;
}

static conn_t *conn_new(uint32_t cip, uint16_t cp, uint32_t sip, uint16_t sp)
{
    int slot = 0;                                   /* 빈 슬롯, 없으면 가장 오래된 것 */
    for (int i = 0; i < MAX_CONN; i++) {
        if (!conns[i].used) { slot = i; break; }
        if (conns[i].last < conns[slot].last) slot = i;
    }
    if (conns[slot].used) conn_free(&conns[slot]);
    conn_t *c = &conns[slot];
    c->used = 1;
    c->cip = cip; c->cport = cp; c->sip = sip; c->sport = sp;
    return c;
}

/* ---------------- 출력 ---------------- */

static const char *pkttype_str(unsigned char t)
{
    switch (t) {
    case PACKET_HOST:     return "RECV(HOST)";
    case PACKET_OUTGOING: return "SEND(OUTGOING)";
    default:              return "OTHER";
    }
}

static void print_summary(const struct sockaddr_ll *sll, const struct ether_header *eth,
                          const struct iphdr *ip, int ihl, const struct tcphdr *tcp,
                          int doff, int plen, int len)
{
    char src[INET_ADDRSTRLEN], dst[INET_ADDRSTRLEN], ifname[IF_NAMESIZE] = "?", ts[16];
    const uint8_t *s = eth->ether_shost, *d = eth->ether_dhost;
    time_t now = time(NULL);

    inet_ntop(AF_INET, &ip->saddr, src, sizeof(src));
    inet_ntop(AF_INET, &ip->daddr, dst, sizeof(dst));
    if_indextoname(sll->sll_ifindex, ifname);
    strftime(ts, sizeof(ts), "%H:%M:%S", localtime(&now));

    printf("============================================================\n");
    printf("[%s] if=%s  dir=%s  len=%d\n", ts, ifname, pkttype_str(sll->sll_pkttype), len);
    printf("  [Ethernet] %02x:%02x:%02x:%02x:%02x:%02x -> %02x:%02x:%02x:%02x:%02x:%02x  type=0x%04x\n",
           s[0], s[1], s[2], s[3], s[4], s[5], d[0], d[1], d[2], d[3], d[4], d[5], ntohs(eth->ether_type));
    printf("  [IP] %s -> %s  hdr=%dB  total=%d  TTL=%d  proto=%d\n",
           src, dst, ihl, ntohs(ip->tot_len), ip->ttl, ip->protocol);
    printf("  [TCP] %u -> %u  hdr=%dB  seq=%u  ack=%u  flags=%s%s%s%s%s\n",
           ntohs(tcp->source), ntohs(tcp->dest), doff, ntohl(tcp->seq), ntohl(tcp->ack_seq),
           tcp->syn ? "SYN " : "", tcp->ack ? "ACK " : "", tcp->psh ? "PSH " : "",
           tcp->fin ? "FIN " : "", tcp->rst ? "RST " : "");
    printf("  payload=%d bytes\n", plen);
}

/* ---------------- 패킷 처리 ---------------- */

static void handle_packet(const unsigned char *buf, int len, const struct sockaddr_ll *sll)
{
    /* L2: Ethernet (14B) - IPv4 만 */
    if (len < (int)sizeof(struct ether_header)) return;
    const struct ether_header *eth = (const struct ether_header *)buf;
    if (ntohs(eth->ether_type) != ETHERTYPE_IP) return;

    /* L3: IPv4 - 헤더 길이는 IHL*4 (가변) */
    const unsigned char *ipp = buf + sizeof(struct ether_header);
    int ip_avail = len - (int)sizeof(struct ether_header);
    if (ip_avail < 20) return;
    const struct iphdr *ip = (const struct iphdr *)ipp;
    int ihl = ip->ihl * 4;
    if (ip->version != 4 || ihl < 20 || ihl > ip_avail || ip->protocol != IPPROTO_TCP) return;
    int ip_total = ntohs(ip->tot_len);
    if (ip_total > ip_avail) ip_total = ip_avail;

    /* L4: TCP - 헤더 길이는 doff*4 (가변) */
    const unsigned char *tcpp = ipp + ihl;
    int tcp_avail = ip_total - ihl;
    if (tcp_avail < 20) return;
    const struct tcphdr *tcp = (const struct tcphdr *)tcpp;
    int doff = tcp->doff * 4;
    if (doff < 20 || doff > tcp_avail) return;

    uint16_t sp = ntohs(tcp->source), dp = ntohs(tcp->dest);
    if (!is_http_port(sp) && !is_http_port(dp)) return;      /* 포트 필터 */

    const unsigned char *payload = tcpp + doff;
    int plen = tcp_avail - doff;
    uint32_t seq = ntohl(tcp->seq);

    /* 연결 찾기/생성 */
    int dir = 0;
    conn_t *c = conn_find(ip->saddr, sp, ip->daddr, dp, &dir);
    if (tcp->syn && !tcp->ack) {                              /* SYN: 새 연결 */
        if (c) conn_free(c);
        c = conn_new(ip->saddr, sp, ip->daddr, dp);
        c->d[0].init = 1; c->d[0].next_seq = seq + 1;
        c->last = time(NULL);
        return;
    }
    if (!c) {
        if (plen <= 0) return;
        if (is_http_port(dp)) { c = conn_new(ip->saddr, sp, ip->daddr, dp); dir = 0; }
        else                  { c = conn_new(ip->daddr, dp, ip->saddr, sp); dir = 1; }
    }
    c->last = time(NULL);
    dir_t *ds = &c->d[dir];
    if (tcp->syn && tcp->ack) { ds->init = 1; ds->next_seq = seq + 1; return; }   /* SYN-ACK */
    if (plen <= 0) { if (tcp->rst) conn_free(c); return; }

    /* TCP 순서 정렬: 이미 본 데이터(재전송, lo 이중 캡처)는 버림 */
    if (!ds->init) { ds->init = 1; ds->next_seq = seq; }
    int32_t diff = (int32_t)(seq - ds->next_seq);
    if (diff + plen <= 0) return;
    if (diff < 0) { payload += -diff; plen += diff; seq = ds->next_seq; }   /* 일부 겹침 */
    if (diff > 0) {                                                         /* 누락 */
        printf("  [!] TCP gap %d bytes (packet loss / capture started mid-stream)\n", diff);
        h2_reset(&ds->h2);
    }
    ds->next_seq = seq + plen;

    /* HTTP/2 고정: 방향별 버퍼에 넣고 완성된 프레임을 출력 */
    print_summary(sll, eth, ip, ihl, tcp, doff, plen, len);
    h2_feed(&ds->h2, payload, plen, dir);
    fflush(stdout);
}

int main(int argc, char *argv[])
{
    const char *ifname = argc > 1 ? argv[1] : NULL;

    int sock = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
    if (sock < 0) { perror("socket (root 권한/CAP_NET_RAW 필요)"); return 1; }

    if (ifname && strcmp(ifname, "any") != 0) {               /* 특정 인터페이스만 */
        struct sockaddr_ll a = {0};
        a.sll_family = AF_PACKET;
        a.sll_protocol = htons(ETH_P_ALL);
        a.sll_ifindex = if_nametoindex(ifname);
        if (a.sll_ifindex == 0) { fprintf(stderr, "interface not found: %s\n", ifname); return 1; }
        if (bind(sock, (struct sockaddr *)&a, sizeof(a)) < 0) { perror("bind"); return 1; }
    }

    struct sigaction sa = {0};
    sa.sa_handler = on_sigint;
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);

    printf("HTTP/2 sniffer started (if=%s, ports=80,8080,8000). Ctrl+C to stop.\n",
           ifname ? ifname : "any");

    unsigned char *buf = malloc(BUF_SIZE);
    while (running) {
        struct sockaddr_ll sll;
        socklen_t sl = sizeof(sll);
        int n = recvfrom(sock, buf, BUF_SIZE, 0, (struct sockaddr *)&sll, &sl);
        if (n > 0) handle_packet(buf, n, &sll);
    }
    free(buf);
    for (int i = 0; i < MAX_CONN; i++) if (conns[i].used) conn_free(&conns[i]);
    close(sock);
    printf("\nstopped.\n");
    return 0;
}
