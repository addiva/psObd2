/* it.massirito.pspobd2 - PSP OBD2 diagnostics & telemetry
 * (c) massirito. Homebrew non firmato crittograficamente: gira su CFW/HEN.
 * File: elm.h  |  Modulo: dialogo ELM327 (AT init, query PID, DTC)
 */
#ifndef ELM_H
#define ELM_H

#include <stdint.h>

typedef enum {
    ELM_DISCONNECTED,
    ELM_CONNECTING,
    ELM_INIT,
    ELM_READY,
    ELM_ERROR
} elm_state_t;

typedef struct {
    int         sock;
    elm_state_t state;
    char        ip[64];
    int         port;
    char        version[32];    /* es. "ELM327 v1.5" */
    char        protocol[48];   /* es. "ISO 15765-4 (CAN 11/500)" */
    int         is_can;         /* 1 CAN, 0 legacy, -1 ignoto */
    int         timeouts;       /* timeout consecutivi (>=3 -> riconnessione) */
    char        last_error[96];
} elm_ctx_t;

#define ELM_TIMEOUT_MS      2500   /* risposta normale */
#define ELM_TIMEOUT_LONG_MS 8000   /* primo comando OBD: ricerca protocollo */

void elm_init(elm_ctx_t *ctx, const char *ip, int port);

/* TCP connect + sequenza AT. 0 OK, -1 errore (vedi last_error) */
int  elm_connect(elm_ctx_t *ctx);

/* Invia un comando e legge fino al prompt '>'.
 * Ritorna byte letti, -1 errore socket, -2 timeout senza prompt. */
int  elm_send_cmd(elm_ctx_t *ctx, const char *cmd, char *buf, int buf_len,
                  unsigned int timeout_ms);

/* Query PID (mode 01). Ritorna n byte dati, 0 nessuna risposta, -1 errore link. */
int  elm_query_pid(elm_ctx_t *ctx, int mode, int pid, uint8_t *out, int max_out);

/* Legge 0100/0120/0140/0160 (OR fra ECU). Ritorna 1 se l'auto risponde,
 * 0 se nessuna ECU risponde (quadro spento?), -1 errore link. */
int  elm_query_supported_pids(elm_ctx_t *ctx, uint8_t supported[16]);

/* Legge protocollo (ATDPN/ATDP) dopo che l'auto ha risposto */
void elm_detect_protocol(elm_ctx_t *ctx);

/* DTC: mode 3 = memorizzati, 7 = in attesa. Ritorna n DTC, -1 errore */
int  elm_read_dtc(elm_ctx_t *ctx, int mode, char (*dtc_buf)[6], int max_dtc);

/* Mode 04 (cancella DTC). 0 OK, -1 errore/rifiutato */
int  elm_clear_dtc(elm_ctx_t *ctx);

/* Tensione batteria (ATRV), -1 se non disponibile */
float elm_read_voltage(elm_ctx_t *ctx);

void elm_disconnect(elm_ctx_t *ctx);

#endif /* ELM_H */
