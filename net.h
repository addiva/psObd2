/* it.massirito.pspobd2 - PSP OBD2 diagnostics & telemetry
 * (c) massirito. Homebrew non firmato crittograficamente: gira su CFW/HEN.
 * File: net.h  |  Modulo: rete PSP (moduli, apctl, socket TCP)
 *
 * NOTA: Le firme esatte delle API variano tra versioni SDK.
 * Verificare contro gli header locali pspdev prima di compilare.
 */
#ifndef NET_H
#define NET_H

/* Inizializza i moduli di rete PSP (sceNet*, sceNetApctl*, sceNetInet*).
 * Deve essere chiamata prima di qualsiasi altra funzione net.
 * Ritorna 0 OK, -1 errore. */
int  net_init(void);

/* Connette al profilo AP salvato nella PSP (index 1-based).
 * Blocca finche' non ottiene IP o supera il timeout.
 * Ritorna 0 OK, -1 errore. */
int  net_apctl_connect(int profile_index);

/* Disconnette dall'AP */
void net_apctl_disconnect(void);

/* Crea un socket TCP e connette a ip:port.
 * Ritorna il socket descriptor, -1 in caso di errore. */
int  net_connect(const char *ip, int port);

/* Send/recv non bloccanti (timeout gestito esternamente).
 * Ritorna byte inviati/ricevuti, -1 errore, 0 niente disponibile (recv). */
int  net_send(int sock, const char *buf, int len);
int  net_recv(int sock, char *buf, int len);

void net_close_socket(int sock);
void net_cleanup(void);

/* Utilita' temporali */
unsigned int net_time_ms(void);
void         net_sleep_ms(unsigned int ms);

#endif /* NET_H */
