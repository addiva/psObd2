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

#endif /* OBD_H */
