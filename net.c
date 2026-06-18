/* it.massirito.pspobd2 - PSP OBD2 diagnostics & telemetry
 * (c) massirito. Homebrew non firmato crittograficamente: gira su CFW/HEN.
 * File: net.c  |  Modulo: rete PSP (moduli, apctl, socket TCP)
 *
 * NOTA: verificare le firme delle sceNet* contro gli header pspdev locali.
 * Le API di rete variano tra versioni firmware/SDK (2.xx vs 6.xx).
 */
#include "net.h"

#ifdef PSP_BUILD
/* ---- Inclusioni PSP-specifiche ---- */
#include <pspkernel.h>
#include <pspnet.h>
#include <pspnet_inet.h>
#include <pspnet_apctl.h>
#include <pspnet_resolver.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <psprtc.h>

/* Timeout connessione AP (ms) */
#define APCTL_TIMEOUT_MS 30000

int net_init(void) {
    int ret;
    /* Carica i moduli di rete - le funzioni esatte dipendono dalla versione SDK */
    /* Verifica gli header locali per sceUtilityLoadNetModule o pspSdkLoadStartModule */
    ret = sceNetInit(128 * 1024, 42, 4 * 1024, 42, 4 * 1024);
    if (ret < 0) return -1;
    ret = sceNetInetInit();
    if (ret < 0) return -1;
    ret = sceNetApctlInit(0x8000, 48);
    if (ret < 0) return -1;
    return 0;
}

int net_apctl_connect(int profile_index) {
    int ret = sceNetApctlConnect(profile_index);
    if (ret < 0) return -1;

    unsigned int t0 = net_time_ms();
    while (net_time_ms() - t0 < APCTL_TIMEOUT_MS) {
        int state = 0;
        sceNetApctlGetState(&state);
        if (state == PSP_NET_APCTL_STATE_GOT_IP) return 0;
        if (state == PSP_NET_APCTL_STATE_DISCONNECTED) return -1;
        net_sleep_ms(50);
    }
    return -1; /* timeout */
}

void net_apctl_disconnect(void) {
    sceNetApctlDisconnect();
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

    /* Imposta socket non-bloccante per recv */
    int flags = 1;
    sceNetInetSetsockopt(sock, SOL_SOCKET, SO_NONBLOCK, &flags, sizeof(flags));
    return sock;
}

int net_send(int sock, const char *buf, int len) {
    return (int)sceNetInetSend(sock, buf, (unsigned)len, 0);
}

int net_recv(int sock, char *buf, int len) {
    int n = (int)sceNetInetRecv(sock, buf, (unsigned)len, 0);
    if (n < 0) {
        int err = sceNetInetGetErrno();
        if (err == EWOULDBLOCK || err == EAGAIN) return 0;
        return -1;
    }
    return n;
}

void net_close_socket(int sock) {
    if (sock >= 0) sceNetInetClose(sock);
}

void net_cleanup(void) {
    sceNetApctlTerm();
    sceNetInetTerm();
    sceNetTerm();
}

unsigned int net_time_ms(void) {
    u64 ticks;
    sceRtcGetCurrentTick(&ticks);
    u32 freq = (u32)sceRtcGetTickResolution();
    return (unsigned int)(ticks * 1000 / freq);
}

void net_sleep_ms(unsigned int ms) {
    sceKernelDelayThread(ms * 1000);
}

#else /* HOST BUILD (test_logic.c / PC) */
#include <time.h>
#include <unistd.h>
#include <string.h>

int  net_init(void) { return 0; }
int  net_apctl_connect(int p) { (void)p; return 0; }
void net_apctl_disconnect(void) {}
int  net_connect(const char *ip, int port) { (void)ip; (void)port; return -1; }
int  net_send(int s, const char *b, int l) { (void)s;(void)b;(void)l; return -1; }
int  net_recv(int s, char *b, int l) { (void)s;(void)b;(void)l; return 0; }
void net_close_socket(int s) { (void)s; }
void net_cleanup(void) {}

unsigned int net_time_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (unsigned int)(ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
}

void net_sleep_ms(unsigned int ms) {
    usleep(ms * 1000);
}
#endif /* PSP_BUILD */
