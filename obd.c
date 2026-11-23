/* it.massirito.pspobd2 - PSP OBD2 diagnostics & telemetry
 * (c) massirito. Homebrew non firmato crittograficamente: gira su CFW/HEN.
 * File: obd.c  |  Modulo: parsing PID OBD2 e decodifica DTC (puro/portabile)
 */
#include "obd.h"
#include <string.h>
#include <ctype.h>

float obd_rpm(const uint8_t *d, int len) {
    if (len < 2) return -1.0f;
    return ((d[0] * 256) + d[1]) / 4.0f;
}

int obd_speed(const uint8_t *d, int len) {
    if (len < 1) return -1;
    return d[0];
}

int obd_coolant_c(const uint8_t *d, int len) {
    if (len < 1) return -1000;
    return d[0] - 40;
}

float obd_maf_gps(const uint8_t *d, int len) {
    if (len < 2) return -1.0f;
    return ((d[0] * 256) + d[1]) / 100.0f;
}

float obd_fuel_level_pct(const uint8_t *d, int len) {
    if (len < 1) return -1.0f;
    return d[0] * 100.0f / 255.0f;
}

int obd_throttle_pct(const uint8_t *d, int len) {
    if (len < 1) return -1;
    return (int)(d[0] * 100 / 255);
}

float obd_fuel_rate_lh(const uint8_t *d, int len) {
    if (len < 2) return -1.0f;
    return ((d[0] * 256) + d[1]) / 20.0f;
}

int obd_mil_status(const uint8_t *d, int len, int *mil_on) {
    if (len < 1) { if (mil_on) *mil_on = 0; return -1; }
    if (mil_on) *mil_on = (d[0] & 0x80) ? 1 : 0;
    return d[0] & 0x7F;
}

int obd_pid_supported(const uint8_t *mask4, int len, int bit_index) {
    if (len < 4 || bit_index < 1 || bit_index > 32) return 0;
    int byte = (bit_index - 1) / 8;
    int bit  = 7 - ((bit_index - 1) % 8);
    return (mask4[byte] >> bit) & 1;
}

void obd_decode_dtc(uint8_t a, uint8_t b, char *out) {
    static const char letters[4] = {'P', 'C', 'B', 'U'};
    const char *hex = "0123456789ABCDEF";
    out[0] = letters[(a >> 6) & 0x03];
    out[1] = hex[(a >> 4) & 0x03];
    out[2] = hex[a & 0x0F];
    out[3] = hex[(b >> 4) & 0x0F];
    out[4] = hex[b & 0x0F];
    out[5] = '\0';
}

static int hex_nibble(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    return -1;
}

int obd_parse_response(const char *response, uint8_t *out, int max_out) {
    if (!response || !out || max_out <= 0) return 0;
    /* Salta: SEARCHING..., NO DATA, ?, echo (linea senza '4') */
    /* Cerca la prima linea che inizia con "4" (risposta modo 4x) */
    const char *p = response;
    int count = 0;
    while (*p && count < max_out) {
        /* salta whitespace e CR/LF */
        while (*p && (isspace((unsigned char)*p) || *p == '>')) p++;
        if (!*p) break;
        /* salta linee che non sono risposte hex OBD */
        if (!isxdigit((unsigned char)*p)) {
            while (*p && *p != '\r' && *p != '\n') p++;
            continue;
        }
        /* leggi coppie hex */
        while (*p && *p != '\r' && *p != '\n' && *p != '>') {
            while (*p == ' ') p++;
            if (!isxdigit((unsigned char)*p)) break;
            int hi = hex_nibble(*p++);
            if (!isxdigit((unsigned char)*p)) break;
            int lo = hex_nibble(*p++);
            if (hi < 0 || lo < 0) break;
            if (count < max_out)
                out[count++] = (uint8_t)((hi << 4) | lo);
        }
        /* avanza alla prossima riga */
        while (*p && *p != '\r' && *p != '\n') p++;
    }
    return count;
}

/* ======================================================================
 * Parser "a righe"
 * ====================================================================== */

static int is_hex_str(const char *s, int n) {
    if (n <= 0) return 0;
    for (int i = 0; i < n; i++)
        if (!isxdigit((unsigned char)s[i])) return 0;
    return 1;
}

static int append_hex(obd_frame_t *f, const char *s, int n, int limit) {
    for (int i = 0; i + 1 < n; i += 2) {
        if (f->n >= limit || f->n >= OBD_FRAME_MAX) break;
        f->b[f->n++] = (uint8_t)((hex_nibble(s[i]) << 4) | hex_nibble(s[i + 1]));
    }
    return f->n;
}

int obd_split_frames(const char *resp, obd_frame_t *fr, int max_fr) {
    if (!resp || !fr || max_fr <= 0) return 0;
    int nfr = 0;
    int mf = -1;          /* indice frame multi-frame in costruzione */
    int mf_len = 0;       /* lunghezza dichiarata */
    const char *p = resp;
    while (*p) {
        /* estrai una riga compattata (senza spazi e '>') */
        char line[160];
        int n = 0;
        while (*p && *p != '\r' && *p != '\n') {
            char c = *p++;
            if (c == ' ' || c == '\t' || c == '>') continue;
            if (n < (int)sizeof(line) - 1) line[n++] = (char)toupper((unsigned char)c);
        }
        while (*p == '\r' || *p == '\n') p++;
        line[n] = '\0';
        if (n == 0) continue;

        /* Header multi-frame CAN: 3 cifre hex = lunghezza totale */
        if (n == 3 && is_hex_str(line, 3)) {
            if (nfr >= max_fr) break;
            mf = nfr++;
            fr[mf].n = 0;
            mf_len = (hex_nibble(line[0]) << 8) | (hex_nibble(line[1]) << 4) |
                     hex_nibble(line[2]);
            continue;
        }
        /* Riga di continuazione "N:hex" */
        if (n >= 2 && line[1] == ':' && isxdigit((unsigned char)line[0])) {
            if (mf >= 0 && is_hex_str(line + 2, n - 2))
                append_hex(&fr[mf], line + 2, n - 2, mf_len);
            continue;
        }
        /* Frame singolo: solo hex, numero pari di cifre */
        if (is_hex_str(line, n) && (n % 2) == 0) {
            if (nfr >= max_fr) break;
            fr[nfr].n = 0;
            append_hex(&fr[nfr], line, n, OBD_FRAME_MAX);
            nfr++;
            mf = -1;
            continue;
        }
        /* testo (SEARCHING..., NO DATA, BUS INIT, ELM327 v1.5, ...) */
        mf = -1;
    }
    return nfr;
}

int obd_extract_pid(const char *resp, int mode, int pid,
                    uint8_t *out, int max_out) {
    obd_frame_t fr[OBD_MAX_FRAMES];
    int nf = obd_split_frames(resp, fr, OBD_MAX_FRAMES);
    for (int i = 0; i < nf; i++) {
        if (fr[i].n < 3) continue;
        if (fr[i].b[0] != (uint8_t)(0x40 + mode) || fr[i].b[1] != (uint8_t)pid)
            continue;
        int n = fr[i].n - 2;
        if (n > max_out) n = max_out;
        memcpy(out, fr[i].b + 2, (size_t)n);
        return n;
    }
    return 0;
}

int obd_supported_mask(const char *resp, int base_pid, uint8_t mask4[4]) {
    obd_frame_t fr[OBD_MAX_FRAMES];
    int nf = obd_split_frames(resp, fr, OBD_MAX_FRAMES);
    int any = 0;
    memset(mask4, 0, 4);
    for (int i = 0; i < nf; i++) {
        if (fr[i].n < 6 || fr[i].b[0] != 0x41 || fr[i].b[1] != (uint8_t)base_pid)
            continue;
        for (int k = 0; k < 4; k++) mask4[k] |= fr[i].b[2 + k];
        any = 1;
    }
    return any;
}

int obd_pid_is_supported(const uint8_t sup[16], int pid) {
    if (!sup || pid < 1 || pid > 0x80) return 0;
    int block = (pid - 1) / 32;            /* 0..3 */
    int bit   = (pid - 1) % 32 + 1;        /* 1..32 */
    return obd_pid_supported(sup + block * 4, 4, bit);
}

int obd_parse_dtcs(const char *resp, int mode, int is_can,
                   char (*out)[6], int max_out) {
    obd_frame_t fr[OBD_MAX_FRAMES];
    int nf = obd_split_frames(resp, fr, OBD_MAX_FRAMES);
    int count = 0;
    for (int i = 0; i < nf; i++) {
        const obd_frame_t *f = &fr[i];
        if (f->n < 1 || f->b[0] != (uint8_t)(0x40 + mode)) continue;
        int can = (is_can == 1) || (is_can < 0 && (f->n % 2) == 0);
        int start, end;
        if (can) {
            if (f->n < 2) continue;
            start = 2;
            end = 2 + 2 * f->b[1];
            if (end > f->n) end = f->n;
        } else {
            start = 1;
            end = f->n;
        }
        for (int k = start; k + 1 < end && count < max_out; k += 2) {
            if (f->b[k] == 0 && f->b[k + 1] == 0) continue;
            char code[6];
            obd_decode_dtc(f->b[k], f->b[k + 1], code);
            int dup = 0;
            for (int j = 0; j < count; j++)
                if (strcmp(out[j], code) == 0) { dup = 1; break; }
            if (!dup) memcpy(out[count++], code, 6);
        }
    }
    return count;
}

float obd_parse_voltage(const char *resp) {
    if (!resp) return -1.0f;
    for (const char *p = resp; *p; p++) {
        if (!isdigit((unsigned char)*p)) continue;
        float v = 0.0f, scale = 0.0f;
        const char *q = p;
        while (isdigit((unsigned char)*q) || *q == '.') {
            if (*q == '.') { if (scale > 0) break; scale = 1.0f; }
            else if (scale > 0) { scale *= 0.1f; v += (*q - '0') * scale; }
            else v = v * 10.0f + (*q - '0');
            q++;
        }
        if (*q == 'V' || *q == 'v') return v;
        p = q - 1 > p ? q - 1 : p;
    }
    return -1.0f;
}

int obd_proto_is_can(const char *dpn_resp) {
    if (!dpn_resp) return -1;
    for (const char *p = dpn_resp; *p; p++) {
        char c = (char)toupper((unsigned char)*p);
        if (c == 'A' && isxdigit((unsigned char)p[1])) continue; /* prefisso "auto" */
        if (isdigit((unsigned char)c) || (c >= 'A' && c <= 'C')) {
            if (c == '0') return -1;
            return (c >= '6' && c <= '9') || (c >= 'A' && c <= 'C');
        }
    }
    return -1;
}
