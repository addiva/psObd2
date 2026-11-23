/* it.massirito.pspobd2 - PSP OBD2 diagnostics & telemetry
 * (c) massirito. Homebrew non firmato crittograficamente: gira su CFW/HEN.
 * File: net.c  |  Modulo: rete PSP (moduli, apctl, socket TCP)
 */
#include "net.h"

#ifdef PSP_BUILD
#include <pspkernel.h>
#include <psputility.h>
#include <pspnet.h>
#include <pspnet_inet.h>
#include <pspnet_apctl.h>
#include <pspwlan.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>

static int s_net_ready = 0;

int net_init(void) {
    if (s_net_ready) return 0;
    /* Su firmware >= 2.00 i moduli di rete vanno caricati esplicitamente
     * (un errore "gia' caricato" e' innocuo, quindi non lo trattiamo come fatale) */
    sceUtilityLoadNetModule(PSP_NET_MODULE_COMMON);
    sceUtilityLoadNetModule(PSP_NET_MODULE_INET);

    int ret = sceNetInit(128 * 1024, 42, 4 * 1024, 42, 4 * 1024);
    if (ret < 0) return ret;
    ret = sceNetInetInit();
    if (ret < 0) { sceNetTerm(); return ret; }
    ret = sceNetApctlInit(0x8000, 48);
    if (ret < 0) { sceNetInetTerm(); sceNetTerm(); return ret; }
    s_net_ready = 1;
    return 0;
}

int net_wlan_switch_on(void) {
    return sceWlanGetSwitchState() != 0;
}

static int apctl_state(void) {
    int state = -1;
    if (sceNetApctlGetState(&state) < 0) return -1;
    return state;
}

int net_apctl_connected(void) {
    return s_net_ready && apctl_state() == PSP_NET_APCTL_STATE_GOT_IP;
}

int net_apctl_connect(int profile_index, unsigned int timeout_ms) {
    if (!s_net_ready) return -1;
    int st = apctl_state();
    if (st == PSP_NET_APCTL_STATE_GOT_IP) return 0;
    if (st > PSP_NET_APCTL_STATE_DISCONNECTED) {
        /* tentativo precedente ancora appeso: ripartiamo puliti */
        sceNetApctlDisconnect();
        unsigned int t = net_time_ms();
        while (apctl_state() > 0 && net_time_ms() - t < 3000) net_sleep_ms(50);
    }
    if (sceNetApctlConnect(profile_index) < 0) return -1;

    unsigned int t0 = net_time_ms();
    int seen_progress = 0;
    while (net_time_ms() - t0 < timeout_ms) {
        st = apctl_state();
        if (st == PSP_NET_APCTL_STATE_GOT_IP) return 0;
        /* Subito dopo Connect lo stato puo' essere ancora 0: e' un errore
         * solo se torna a 0 DOPO essere avanzato (AP assente / password errata) */
        if (st > PSP_NET_APCTL_STATE_DISCONNECTED) seen_progress = 1;
        else if (seen_progress) return -2;
        net_sleep_ms(50);
    }
    sceNetApctlDisconnect();
    return -3;
}

void net_local_ip(char *out, int len) {
    if (!out || len <= 0) return;
    out[0] = '\0';
    union SceNetApctlInfo info;
    memset(&info, 0, sizeof(info));
    if (s_net_ready && sceNetApctlGetInfo(PSP_NET_APCTL_INFO_IP, &info) == 0) {
        strncpy(out, info.ip, (size_t)len - 1);
        out[len - 1] = '\0';
    }
}

void net_apctl_disconnect(void) {
    if (s_net_ready) sceNetApctlDisconnect();
}

int net_connect(const char *ip, int port) {
    int sock = sceNetInetSocket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) return -1;

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port   = htons((unsigned short)port);
    addr.sin_addr.s_addr = inet_addr(ip);

    if (sceNetInetConnect(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        sceNetInetClose(sock);
        return -1;
    }
    int one = 1;
    /* Comandi ELM brevi: niente algoritmo di Nagle */
    sceNetInetSetsockopt(sock, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one));
    /* recv non bloccante (timeout gestito da elm.c) */
    sceNetInetSetsockopt(sock, SOL_SOCKET, SO_NONBLOCK, &one, sizeof(one));
    return sock;
}

int net_send(int sock, const char *buf, int len) {
    int sent = 0;
    unsigned int t0 = net_time_ms();
    while (sent < len) {
        int n = (int)sceNetInetSend(sock, buf + sent, (unsigned)(len - sent), 0);
        if (n < 0) {
            int err = sceNetInetGetErrno();
            if ((err == EWOULDBLOCK || err == EAGAIN) && net_time_ms() - t0 < 2000) {
                net_sleep_ms(5);
                continue;
            }
            return -1;
        }
        sent += n;
    }
    return sent;
}

int net_recv(int sock, char *buf, int len) {
    int n = (int)sceNetInetRecv(sock, buf, (unsigned)len, 0);
    if (n < 0) {
        int err = sceNetInetGetErrno();
        if (err == EWOULDBLOCK || err == EAGAIN) return 0;
        return -1;
    }
    if (n == 0) return -1;   /* connessione chiusa dall'adattatore */
    return n;
}

void net_close_socket(int sock) {
    if (sock >= 0) sceNetInetClose(sock);
}

void net_cleanup(void) {
    if (!s_net_ready) return;
    sceNetApctlTerm();
    sceNetInetTerm();
    sceNetTerm();
    s_net_ready = 0;
}

unsigned int net_time_ms(void) {
    /* microsecondi dall'avvio (64 bit): nessun overflow */
    return (unsigned int)(sceKernelGetSystemTimeWide() / 1000);
}

void net_sleep_ms(unsigned int ms) {
    sceKernelDelayThread(ms * 1000);
}

#else /* ===================== HOST (PC): socket POSIX ===================== */
#include <time.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>

int  net_init(void) { return 0; }
int  net_wlan_switch_on(void) { return 1; }
int  net_apctl_connect(int p, unsigned int t) { (void)p; (void)t; return 0; }
int  net_apctl_connected(void) { return 1; }
void net_local_ip(char *out, int len) { if (out && len > 0) { strncpy(out, "127.0.0.1", (size_t)len - 1); out[len - 1] = 0; } }
void net_apctl_disconnect(void) {}

int net_connect(const char *ip, int port) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) return -1;
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons((unsigned short)port);
    addr.sin_addr.s_addr = inet_addr(ip);
    if (connect(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) { close(sock); return -1; }
    int one = 1;
    setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one));
    fcntl(sock, F_SETFL, fcntl(sock, F_GETFL, 0) | O_NONBLOCK);
    return sock;
}

int net_send(int s, const char *b, int l) { return (int)send(s, b, (size_t)l, 0); }

int net_recv(int s, char *b, int l) {
    ssize_t n = recv(s, b, (size_t)l, 0);
    if (n < 0) return (errno == EAGAIN || errno == EWOULDBLOCK) ? 0 : -1;
    if (n == 0) return -1;
    return (int)n;
}

void net_close_socket(int s) { if (s >= 0) close(s); }
void net_cleanup(void) {}

unsigned int net_time_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (unsigned int)(ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
}

void net_sleep_ms(unsigned int ms) { usleep(ms * 1000); }
#endif /* PSP_BUILD */
