/* it.massirito.pspobd2 - PSP OBD2 diagnostics & telemetry
 * (c) massirito. Homebrew non firmato crittograficamente: gira su CFW/HEN.
 * File: elm.h  |  Modulo: dialogo ELM327 (AT init, query PID, parse risposta)
 */
#ifndef ELM_H
#define ELM_H

#include <stdint.h>

/* Stato connessione ELM */
typedef enum {
    ELM_DISCONNECTED,
    ELM_CONNECTING,
    ELM_INIT,        /* invio comandi AT* */
    ELM_READY,
    ELM_ERROR
} elm_state_t;

typedef struct {
    int        sock;       /* socket TCP (-1 = non connesso) */
    elm_state_t state;
    char       ip[64];
    int        port;
    char       last_error[128];
} elm_ctx_t;

/* Timeout ricezione risposta ELM (ms) */
#define ELM_TIMEOUT_MS 2000

/* Inizializza il contesto */
void elm_init(elm_ctx_t *ctx, const char *ip, int port);

/* Connette il socket TCP e invia la sequenza AT di init.
 * Ritorna 0 OK, -1 errore. Usa sock gia' valido in ctx->sock. */
int  elm_connect(elm_ctx_t *ctx);

/* Invia un comando (aggiunge '\r' automaticamente) e attende la risposta
 * terminata da '>'. Risposta in buf[buf_len]. Ritorna byte letti, -1 errore. */
int  elm_send_cmd(elm_ctx_t *ctx, const char *cmd, char *buf, int buf_len);

/* Interroga un PID OBD (es. "010C") e scrive i byte dati in out.
 * Salta echo, modo, PID dalla risposta.
 * Ritorna numero di byte dati, 0 se NO DATA, -1 errore. */
int  elm_query_pid(elm_ctx_t *ctx, const char *pid_str,
                   uint8_t *out, int max_out);

/* Invia Mode 03 (leggi DTC). Ritorna numero di DTC ricevuti.
 * dtc_buf: array di char[6], max_dtc elementi. */
int  elm_read_dtc(elm_ctx_t *ctx, char (*dtc_buf)[6], int max_dtc);

/* Invia Mode 04 (cancella DTC). Ritorna 0 OK, -1 errore. */
int  elm_clear_dtc(elm_ctx_t *ctx);

/* Interroga 0100/0120/0140/0160 e popola la bitmask PID (16 byte totali).
 * Ritorna 0 OK. */
int  elm_query_supported_pids(elm_ctx_t *ctx, uint8_t supported[16]);

void elm_disconnect(elm_ctx_t *ctx);

#endif /* ELM_H */
