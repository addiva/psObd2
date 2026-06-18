/* it.massirito.pspobd2 - PSP OBD2 diagnostics & telemetry
 * (c) massirito. Homebrew non firmato crittograficamente: gira su CFW/HEN.
 * File: elm.c  |  Modulo: dialogo ELM327 (AT init, query PID, parse risposta)
 */
#include "elm.h"
#include "obd.h"
#include "net.h"
#include <string.h>
#include <stdio.h>

/* Sequenza init ELM327 - ogni comando attende risposta */
static const char * const ELM_INIT_CMDS[] = {
    "ATZ",    /* reset */
    "ATE0",   /* echo off */
    "ATL0",   /* linefeed off */
    "ATS0",   /* spaces off */
    "ATH0",   /* headers off */
    "ATSP0",  /* protocollo auto */
    "ATAT1",  /* adaptive timing on */
    NULL
};

void elm_init(elm_ctx_t *ctx, const char *ip, int port) {
    memset(ctx, 0, sizeof(*ctx));
    ctx->sock  = -1;
    ctx->state = ELM_DISCONNECTED;
    strncpy(ctx->ip, ip, sizeof(ctx->ip) - 1);
    ctx->port = port;
}

int elm_send_cmd(elm_ctx_t *ctx, const char *cmd, char *buf, int buf_len) {
    if (ctx->sock < 0 || !cmd || !buf || buf_len < 2) return -1;

    /* Invia comando + \r */
    char send_buf[128];
    int slen = snprintf(send_buf, sizeof(send_buf) - 1, "%s\r", cmd);
    if (net_send(ctx->sock, send_buf, slen) < 0) {
        ctx->state = ELM_ERROR;
        return -1;
    }

    /* Leggi fino al prompt '>' o timeout */
    int total = 0;
    unsigned int deadline = net_time_ms() + ELM_TIMEOUT_MS;
    while (total < buf_len - 1) {
        if (net_time_ms() > deadline) break;
        int n = net_recv(ctx->sock, buf + total, buf_len - 1 - total);
        if (n < 0) { ctx->state = ELM_ERROR; return -1; }
        if (n == 0) { net_sleep_ms(5); continue; }
        total += n;
        /* termina quando vediamo '>' */
        for (int i = 0; i < total; i++)
            if (buf[i] == '>') { buf[total] = '\0'; return total; }
    }
    buf[total] = '\0';
    return total;
}

int elm_connect(elm_ctx_t *ctx) {
    if (ctx->sock >= 0) net_close_socket(ctx->sock);
    ctx->sock  = -1;
    ctx->state = ELM_CONNECTING;

    ctx->sock = net_connect(ctx->ip, ctx->port);
    if (ctx->sock < 0) {
        ctx->state = ELM_ERROR;
        snprintf(ctx->last_error, sizeof(ctx->last_error),
                 "TCP connect failed: %s:%d", ctx->ip, ctx->port);
        return -1;
    }

    ctx->state = ELM_INIT;
    char rbuf[256];

    for (int i = 0; ELM_INIT_CMDS[i]; i++) {
        /* ATZ ha bisogno di piu' tempo */
        if (i == 0) net_sleep_ms(500);
        int n = elm_send_cmd(ctx, ELM_INIT_CMDS[i], rbuf, sizeof(rbuf));
        if (n < 0) {
            ctx->state = ELM_ERROR;
            snprintf(ctx->last_error, sizeof(ctx->last_error),
                     "ELM init failed at %s", ELM_INIT_CMDS[i]);
            return -1;
        }
    }

    ctx->state = ELM_READY;
    return 0;
}

int elm_query_pid(elm_ctx_t *ctx, const char *pid_str,
                  uint8_t *out, int max_out) {
    if (ctx->state != ELM_READY) return -1;
    char rbuf[256];
    int n = elm_send_cmd(ctx, pid_str, rbuf, sizeof(rbuf));
    if (n < 0) return -1;

    /* Controlla NO DATA */
    if (strstr(rbuf, "NO DATA") || strstr(rbuf, "UNABLE") ||
        strstr(rbuf, "ERROR"))
        return 0;

    /* Parsa la risposta hex */
    uint8_t tmp[32];
    int nb = obd_parse_response(rbuf, tmp, sizeof(tmp));
    if (nb < 2) return 0;

    /* Salta i byte di modo (41) e PID */
    int skip = 2;
    nb -= skip;
    if (nb <= 0 || nb > max_out) return 0;
    memcpy(out, tmp + skip, nb);
    return nb;
}

int elm_read_dtc(elm_ctx_t *ctx, char (*dtc_buf)[6], int max_dtc) {
    if (ctx->state != ELM_READY || !dtc_buf || max_dtc <= 0) return -1;
    char rbuf[512];
    int n = elm_send_cmd(ctx, "03", rbuf, sizeof(rbuf));
    if (n < 0) return -1;
    if (strstr(rbuf, "NO DATA") || strstr(rbuf, "43 00")) return 0;

    uint8_t raw[128];
    int nb = obd_parse_response(rbuf, raw, sizeof(raw));
    if (nb < 1) return 0;

    /* Risposta Mode 03: byte 0 = 0x43, poi coppie di byte DTC */
    int start = (nb > 0 && raw[0] == 0x43) ? 1 : 0;
    int count = 0;
    for (int i = start; i + 1 < nb && count < max_dtc; i += 2) {
        if (raw[i] == 0x00 && raw[i+1] == 0x00) continue; /* padding */
        obd_decode_dtc(raw[i], raw[i+1], dtc_buf[count]);
        count++;
    }
    return count;
}

int elm_clear_dtc(elm_ctx_t *ctx) {
    if (ctx->state != ELM_READY) return -1;
    char rbuf[64];
    elm_send_cmd(ctx, "04", rbuf, sizeof(rbuf));
    /* risposta attesa: "44" */
    if (strstr(rbuf, "44")) return 0;
    return -1;
}

int elm_query_supported_pids(elm_ctx_t *ctx, uint8_t supported[16]) {
    if (!supported) return -1;
    memset(supported, 0, 16);
    const char *queries[] = { "0100", "0120", "0140", "0160", NULL };
    for (int i = 0; queries[i]; i++) {
        uint8_t out[4];
        int n = elm_query_pid(ctx, queries[i], out, 4);
        if (n == 4)
            memcpy(supported + i * 4, out, 4);
    }
    return 0;
}

void elm_disconnect(elm_ctx_t *ctx) {
    if (ctx->sock >= 0) {
        net_close_socket(ctx->sock);
        ctx->sock = -1;
    }
    ctx->state = ELM_DISCONNECTED;
}
