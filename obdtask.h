/* it.massirito.pspobd2 - PSP OBD2 diagnostics & telemetry
 * (c) massirito. Homebrew non firmato crittograficamente: gira su CFW/HEN.
 * File: obdtask.h  |  Modulo: thread OBD (connessione, polling, DTC)
 *
 * Tutto il traffico di rete gira in questo thread: la UI non si blocca mai.
 * La UI comunica solo tramite i flag req_* e legge i campi pubblicati.
 */
#ifndef OBDTASK_H
#define OBDTASK_H

#include "live.h"
#include "elm.h"
#include "profile.h"
#include "fuel.h"
#include "accel.h"

typedef enum {
    LINK_IDLE,
    LINK_WIFI_OFF,     /* switch WLAN spento */
    LINK_WIFI,         /* connessione al profilo di rete PSP */
    LINK_TCP,          /* connessione TCP + init ELM327 */
    LINK_ECU_WAIT,     /* ELM ok, l'auto non risponde ancora */
    LINK_READY,
    LINK_ERROR         /* attesa prima di riprovare */
} link_state_t;

#define DTC_MAX 32

typedef struct {
    /* ---- configurazione (scritta dalla UI, poi req_reconnect = 1) ---- */
    char ip[64];
    int  port;
    int  apctl;
    const car_profile_t * volatile profile;

    /* ---- stato pubblicato dal thread ---- */
    volatile link_state_t link;
    char  status[96];
    char  local_ip[20];
    elm_ctx_t elm;
    car_caps_t caps;
    uint8_t sup[16];
    live_t live;
    volatile unsigned int live_seq;   /* ++ ad ogni ciclo completato */
    float poll_hz;
    trip_t trip;

    accel_run_t  accel;
    accel_best_t best;
    volatile int best_seq;            /* ++ quando un best migliora */

    char  dtc[DTC_MAX][6];
    unsigned char dtc_pending[DTC_MAX]; /* 1 = codice "in attesa" (mode 07) */
    volatile int dtc_count;
    volatile int dtc_valid;
    volatile unsigned int dtc_seq;
    volatile int dtc_busy;
    char  dtc_msg[80];

    /* ---- richieste dalla UI ---- */
    volatile int req_read_dtc;
    volatile int req_clear_dtc;
    volatile int req_reconnect;
    volatile int req_accel_arm;
    volatile int req_trip_reset;
    volatile int req_quit;
    volatile int fast_mode;           /* 1 = solo velocita' (dash 0-100) */
    volatile int running;
} obd_task_t;

void obdtask_init(obd_task_t *t, const char *ip, int port, int apctl,
                  const car_profile_t *profile);

/* Un passo della macchina a stati (bloccante per al massimo qualche secondo).
 * Usata dal thread PSP e dai test su PC. */
void obdtask_step(obd_task_t *t);

/* PSP: avvia/ferma il thread. Su PC start ritorna -1 (usare obdtask_step). */
int  obdtask_start(obd_task_t *t);
void obdtask_stop(obd_task_t *t);

const char *obdtask_link_name(link_state_t s);

#endif /* OBDTASK_H */
