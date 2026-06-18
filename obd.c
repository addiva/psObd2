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
