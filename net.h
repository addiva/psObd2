/* it.massirito.pspobd2 - PSP OBD2 diagnostics & telemetry
 * (c) massirito. Homebrew non firmato crittograficamente: gira su CFW/HEN.
 * File: net.h  |  Modulo: rete PSP (moduli, apctl, socket TCP)
 */
#ifndef NET_H
#define NET_H

/* Carica i moduli di rete firmware (sceUtilityLoadNetModule) e inizializza
 * sceNet / sceNetInet / sceNetApctl. Idempotente. Ritorna 0 OK, <0 errore. */
int  net_init(void);

/* 1 se lo switch WLAN laterale della PSP e' acceso */
int  net_wlan_switch_on(void);

/* Connette al profilo di rete PSP (Impostazioni > Rete, indice 1-based).
 * Bloccante (max timeout_ms). Ritorna 0 OK, -1 errore avvio, -2 connessione
 * rifiutata/AP non trovato, -3 timeout. */
int  net_apctl_connect(int profile_index, unsigned int timeout_ms);

/* 1 se gia' connessi all'AP con IP */
int  net_apctl_connected(void);

/* IP assegnato alla PSP (stringa vuota se non disponibile) */
void net_local_ip(char *out, int len);

void net_apctl_disconnect(void);

/* Socket TCP verso ip:port (connect bloccante, poi recv non bloccante).
 * Ritorna il socket, -1 errore. */
int  net_connect(const char *ip, int port);

/* Ritorna byte inviati/ricevuti, -1 errore, 0 niente disponibile (recv). */
int  net_send(int sock, const char *buf, int len);
int  net_recv(int sock, char *buf, int len);

void net_close_socket(int sock);
void net_cleanup(void);

/* Tempo monotono in ms dall'avvio (wrap-safe con differenze unsigned) */
unsigned int net_time_ms(void);
void         net_sleep_ms(unsigned int ms);

#endif /* NET_H */
