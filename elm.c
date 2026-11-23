/* it.massirito.pspobd2 - PSP OBD2 diagnostics & telemetry
 * (c) massirito. Homebrew non firmato crittograficamente: gira su CFW/HEN.
 * File: elm.c  |  Modulo: dialogo ELM327 (AT init, query PID, DTC)
 */
#include "elm.h"
#include "obd.h"
#include "net.h"
#include <string.h>
#include <stdio.h>
#include <ctype.h>

static const char * const ELM_INIT_CMDS[] = {
    "ATE0",   /* echo off */
    "ATL0",   /* linefeed off */
    "ATS0",   /* spazi off */
    "ATH0",   /* header off */
    "ATSP0",  /* protocollo automatico */
    "ATAT1",  /* timing adattivo */
    NULL
};

void elm_init(elm_ctx_t *ctx, const char *ip, int port) {
    memset(ctx, 0, sizeof(*ctx));
    ctx->sock   = -1;
    ctx->state  = ELM_DISCONNECTED;
    ctx->is_can = -1;
    strncpy(ctx->ip, ip ? ip : "", sizeof(ctx->ip) - 1);
    ctx->port = port;
}

static void set_error(elm_ctx_t *ctx, const char *msg) {
    strncpy(ctx->last_error, msg, sizeof(ctx->last_error) - 1);
    ctx->last_error[sizeof(ctx->last_error) - 1] = '\0';
}

/* Scarta dati arrivati in ritardo (es. risposta di un comando andato in timeout) */
static void drain(elm_ctx_t *ctx) {
    char junk[128];
    for (int i = 0; i < 16; i++)
        if (net_recv(ctx->sock, junk, sizeof(junk)) <= 0) break;
}

int elm_send_cmd(elm_ctx_t *ctx, const char *cmd, char *buf, int buf_len,
                 unsigned int timeout_ms) {
    if (ctx->sock < 0 || !cmd || !buf || buf_len < 2) return -1;
    drain(ctx);

    char send_buf[64];
    int slen = snprintf(send_buf, sizeof(send_buf), "%s\r", cmd);
    if (net_send(ctx->sock, send_buf, slen) < 0) {
        ctx->state = ELM_ERROR;
        set_error(ctx, "Invio al ELM fallito");
        return -1;
    }

    int total = 0;
    unsigned int t0 = net_time_ms();
    buf[0] = '\0';
    while (net_time_ms() - t0 < timeout_ms) {
        int room = buf_len - 1 - total;
        if (room <= 0) {            /* buffer pieno: tieni la coda */
            int keep = buf_len / 2;
            memmove(buf, buf + total - keep, (size_t)keep);
            total = keep;
            room = buf_len - 1 - total;
        }
        int n = net_recv(ctx->sock, buf + total, room);
        if (n < 0) {
            ctx->state = ELM_ERROR;
            set_error(ctx, "Connessione ELM persa");
            return -1;
        }
        if (n == 0) { net_sleep_ms(5); continue; }
        int start = total;
        total += n;
        buf[total] = '\0';
        for (int i = start; i < total; i++)
            if (buf[i] == '>') {
                ctx->timeouts = 0;
                return total;
            }
    }
    ctx->timeouts++;
    return -2;
}

int elm_connect(elm_ctx_t *ctx) {
    if (ctx->sock >= 0) net_close_socket(ctx->sock);
    ctx->sock = -1;
    ctx->state = ELM_CONNECTING;
    ctx->is_can = -1;
    ctx->timeouts = 0;
    ctx->version[0] = ctx->protocol[0] = '\0';

    ctx->sock = net_connect(ctx->ip, ctx->port);
    if (ctx->sock < 0) {
        ctx->state = ELM_ERROR;
        snprintf(ctx->last_error, sizeof(ctx->last_error),
                 "TCP %s:%d non raggiungibile", ctx->ip, ctx->port);
        return -1;
    }

    ctx->state = ELM_INIT;
    char rbuf[256];
    /* Alcuni adattatori inviano un prompt/banner appena connessi:
     * lascialo arrivare e scartalo, altrimenti verrebbe preso come risposta ad ATZ */
    net_sleep_ms(300);
    drain(ctx);
    /* Reset: alcuni cloni impiegano ~1 s; riprova una volta */
    int n = elm_send_cmd(ctx, "ATZ", rbuf, sizeof(rbuf), 3000);
    if (n == -2) n = elm_send_cmd(ctx, "ATZ", rbuf, sizeof(rbuf), 3000);
    if (n < 0) {
        ctx->state = ELM_ERROR;
        set_error(ctx, n == -2 ? "ELM non risponde ad ATZ" : "Errore socket su ATZ");
        return -1;
    }
    /* Versione: riga che contiene "ELM" */
    const char *v = strstr(rbuf, "ELM");
    if (v) {
        int i = 0;
        while (v[i] && v[i] != '\r' && v[i] != '\n' && v[i] != '>' &&
               i < (int)sizeof(ctx->version) - 1) {
            ctx->version[i] = v[i]; i++;
        }
        ctx->version[i] = '\0';
    }
    for (int i = 0; ELM_INIT_CMDS[i]; i++) {
        n = elm_send_cmd(ctx, ELM_INIT_CMDS[i], rbuf, sizeof(rbuf), ELM_TIMEOUT_MS);
        if (n < 0) {
            ctx->state = ELM_ERROR;
            snprintf(ctx->last_error, sizeof(ctx->last_error),
                     "Init ELM fallito su %s", ELM_INIT_CMDS[i]);
            return -1;
        }
    }
    ctx->state = ELM_READY;
    return 0;
}

int elm_query_pid(elm_ctx_t *ctx, int mode, int pid, uint8_t *out, int max_out) {
    if (ctx->state != ELM_READY) return -1;
    char cmd[8], rbuf[384];
    snprintf(cmd, sizeof(cmd), "%02X%02X", mode & 0xFF, pid & 0xFF);
    int n = elm_send_cmd(ctx, cmd, rbuf, sizeof(rbuf), ELM_TIMEOUT_MS);
    if (n == -1) return -1;
    if (n == -2) return 0;
    return obd_extract_pid(rbuf, mode, pid, out, max_out);
}

int elm_query_supported_pids(elm_ctx_t *ctx, uint8_t supported[16]) {
    memset(supported, 0, 16);
    if (ctx->state != ELM_READY) return -1;
    char rbuf[512];
    int any = 0;
    for (int blk = 0; blk < 4; blk++) {
        char cmd[8];
        snprintf(cmd, sizeof(cmd), "01%02X", blk * 0x20);
        /* il primo comando OBD puo' durare secondi (ricerca protocollo) */
        int n = elm_send_cmd(ctx, cmd, rbuf, sizeof(rbuf),
                             blk == 0 ? ELM_TIMEOUT_LONG_MS : ELM_TIMEOUT_MS);
        if (n == -1) return -1;
        if (n < 0) break;
        uint8_t m[4];
        if (!obd_supported_mask(rbuf, blk * 0x20, m)) {
            if (blk == 0) {
                if (strstr(rbuf, "UNABLE") || strstr(rbuf, "NO DATA") ||
                    strstr(rbuf, "BUS INIT") || strstr(rbuf, "ERROR"))
                    set_error(ctx, "L'auto non risponde (quadro acceso?)");
                else
                    set_error(ctx, "Risposta 0100 non valida");
            }
            break;
        }
        memcpy(supported + blk * 4, m, 4);
        any = 1;
        if (!(m[3] & 0x01)) break;      /* bit 32: blocco successivo supportato? */
    }
    return any;
}

void elm_detect_protocol(elm_ctx_t *ctx) {
    char rbuf[128];
    if (elm_send_cmd(ctx, "ATDPN", rbuf, sizeof(rbuf), ELM_TIMEOUT_MS) > 0)
        ctx->is_can = obd_proto_is_can(rbuf);
    if (elm_send_cmd(ctx, "ATDP", rbuf, sizeof(rbuf), ELM_TIMEOUT_MS) > 0) {
        const char *p = rbuf;
        while (*p == '\r' || *p == '\n' || *p == ' ') p++;
        if (strncmp(p, "AUTO, ", 6) == 0) p += 6;
        int i = 0;
        while (p[i] && p[i] != '\r' && p[i] != '\n' && p[i] != '>' &&
               i < (int)sizeof(ctx->protocol) - 1) {
            ctx->protocol[i] = p[i]; i++;
        }
        ctx->protocol[i] = '\0';
    }
}

int elm_read_dtc(elm_ctx_t *ctx, int mode, char (*dtc_buf)[6], int max_dtc) {
    if (ctx->state != ELM_READY || !dtc_buf || max_dtc <= 0) return -1;
    char cmd[4], rbuf[768];
    snprintf(cmd, sizeof(cmd), "%02X", mode & 0xFF);
    int n = elm_send_cmd(ctx, cmd, rbuf, sizeof(rbuf), ELM_TIMEOUT_LONG_MS);
    if (n < 0) return -1;
    return obd_parse_dtcs(rbuf, mode, ctx->is_can, dtc_buf, max_dtc);
}

int elm_clear_dtc(elm_ctx_t *ctx) {
    if (ctx->state != ELM_READY) return -1;
    char rbuf[256];
    int n = elm_send_cmd(ctx, "04", rbuf, sizeof(rbuf), ELM_TIMEOUT_LONG_MS);
    if (n < 0) return -1;
    obd_frame_t fr[OBD_MAX_FRAMES];
    int nf = obd_split_frames(rbuf, fr, OBD_MAX_FRAMES);
    for (int i = 0; i < nf; i++)
        if (fr[i].n >= 1 && fr[i].b[0] == 0x44) return 0;
    return -1;
}

float elm_read_voltage(elm_ctx_t *ctx) {
    if (ctx->state != ELM_READY) return -1.0f;
    char rbuf[64];
    if (elm_send_cmd(ctx, "ATRV", rbuf, sizeof(rbuf), ELM_TIMEOUT_MS) <= 0)
        return -1.0f;
    return obd_parse_voltage(rbuf);
}

void elm_disconnect(elm_ctx_t *ctx) {
    if (ctx->sock >= 0) {
        net_close_socket(ctx->sock);
        ctx->sock = -1;
    }
    ctx->state = ELM_DISCONNECTED;
}
