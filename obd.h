/* it.massirito.pspobd2 - PSP OBD2 diagnostics & telemetry
 * (c) massirito. Homebrew non firmato crittograficamente: gira su CFW/HEN.
 * File: obd.h  |  Modulo: parsing PID OBD2 e decodifica DTC (puro/portabile)
 */
#ifndef OBD_H
#define OBD_H

#include <stdint.h>

/* Tutte le funzioni ricevono i BYTE GIA' DECODIFICATI dalla risposta esadecimale
 * dell'ELM327 (es. "41 0C 1A F8" -> data = {0x1A, 0xF8}, len = 2).
 * Restituiscono -1.0f / valori sentinella se i dati non bastano. */

float obd_rpm(const uint8_t *d, int len);           /* giri/min   PID 010C */
int   obd_speed(const uint8_t *d, int len);         /* km/h       PID 010D */
int   obd_coolant_c(const uint8_t *d, int len);     /* gradi C    PID 0105 */
float obd_maf_gps(const uint8_t *d, int len);       /* g/s aria   PID 0110 */
float obd_fuel_level_pct(const uint8_t *d, int len);/* %serbatoio PID 012F */
int   obd_throttle_pct(const uint8_t *d, int len);  /* %farfalla  PID 0111 */
float obd_fuel_rate_lh(const uint8_t *d, int len);  /* L/h diretto PID 015E */

/* MIL (spia motore) e numero DTC memorizzati - PID 0101.
 * mil_on = 1 se la spia e' accesa. Ritorna il numero di DTC. */
int   obd_mil_status(const uint8_t *d, int len, int *mil_on);

/* Bitmask PID supportati (0100/0120/0140...). bit_index 1..32 relativo al blocco.
 * Ritorna 1 se supportato. */
int   obd_pid_supported(const uint8_t *mask4, int len, int bit_index);

/* Decodifica un DTC da 2 byte in stringa tipo "P0301". out deve essere >=6 char. */
void  obd_decode_dtc(uint8_t a, uint8_t b, char *out);

/* Converte risposta hex ELM327 in byte. Ritorna numero di byte scritti.
 * Salta spazi, echo ("010C"), "SEARCHING...", "NO DATA", "?", prompt '>'.
 * response = es. "41 0C 1A F8 \r\n>" */
int   obd_parse_response(const char *response, uint8_t *out, int max_out);


/* ---------- Parser "a righe" (robusto: piu' ECU, CAN multi-frame) ---------- */

#define OBD_FRAME_MAX  96   /* byte max per frame (anche multi-frame CAN) */
#define OBD_MAX_FRAMES 8

typedef struct {
    uint8_t b[OBD_FRAME_MAX];
    int     n;
} obd_frame_t;

/* Divide una risposta ELM327 in frame (una riga hex = un frame; le sequenze
 * CAN ISO-TP "00A" + "0:..." "1:..." vengono riassemblate in un frame unico).
 * Ignora SEARCHING..., NO DATA, BUS INIT, echo testuali, prompt '>'.
 * Ritorna il numero di frame. */
int   obd_split_frames(const char *resp, obd_frame_t *fr, int max_fr);

/* Cerca il primo frame che risponde a (mode, pid) - es. 0x01,0x0C -> "41 0C ..."
 * e copia i byte DATI (dopo modo e PID). Ritorna n byte, 0 se nessuna risposta
 * valida (NO DATA, risposta di un altro PID, errore). */
int   obd_extract_pid(const char *resp, int mode, int pid,
                      uint8_t *out, int max_out);

/* Risposta a 0100/0120/...: OR delle bitmask di tutte le ECU che rispondono.
 * Ritorna 1 se almeno una ECU ha risposto. */
int   obd_supported_mask(const char *resp, int base_pid, uint8_t mask4[4]);

/* sup = 16 byte (blocchi 0100,0120,0140,0160). pid = 0x01..0x80 */
int   obd_pid_is_supported(const uint8_t sup[16], int pid);

/* Decodifica DTC da risposta Mode 03 / 07 / 0A.
 * is_can: 1 = CAN (byte conteggio dopo 43), 0 = legacy, -1 = autodetect.
 * Rimuove padding 0000 e duplicati (piu' ECU). Ritorna il numero di DTC. */
int   obd_parse_dtcs(const char *resp, int mode, int is_can,
                     char (*out)[6], int max_out);

/* "12.4V" -> 12.4 ; ritorna -1 se non trovato */
float obd_parse_voltage(const char *resp);

/* Risposta ATDPN ("A6", "6", "3"...) -> 1 se protocollo CAN, 0 se no, -1 ignoto */
int   obd_proto_is_can(const char *dpn_resp);

#endif /* OBD_H */
