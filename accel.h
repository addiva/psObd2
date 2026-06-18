/* it.massirito.pspobd2 - PSP OBD2 diagnostics & telemetry
 * (c) massirito. Homebrew non firmato crittograficamente: gira su CFW/HEN.
 * File: accel.h  |  Modulo: cronometro accelerazione 0-20-50-100-120-150-200
 */
#ifndef ACCEL_H
#define ACCEL_H

/* Soglie in km/h (indice = 0..N_THRESHOLDS-1) */
#define ACCEL_THRESHOLDS_N 7
extern const int accel_thresholds[ACCEL_THRESHOLDS_N]; /* {20,50,100,120,150,200} + 0 */

typedef struct {
    int   active;                          /* 1 = run in corso */
    float t_start_s;                       /* timestamp assoluto avvio (s) */
    float t_hit[ACCEL_THRESHOLDS_N];       /* tempo al raggiungimento di ciascuna soglia */
    int   hit[ACCEL_THRESHOLDS_N];         /* 1 = soglia raggiunta */
    int   prev_speed;                      /* velocita' campione precedente */
    float prev_time_s;                     /* timestamp campione precedente */
} accel_run_t;

typedef struct {
    float best[ACCEL_THRESHOLDS_N];       /* best times (0 = non ancora) */
} accel_best_t;

/* Azzera la corsa corrente */
void accel_reset(accel_run_t *r);

/* Inizia la misurazione. now_s = timestamp corrente in secondi */
void accel_start(accel_run_t *r, float now_s);

/* Aggiorna con nuovo campione velocita'. Interpola tra campioni per ridurre errore.
 * Ritorna 1 se tutte le soglie sono state raggiunte (run completato). */
int  accel_update(accel_run_t *r, int speed_kmh, float now_s);

/* Aggiorna i best times se la corsa corrente ha battuto qualcosa */
void accel_update_best(accel_best_t *best, const accel_run_t *r);

#endif /* ACCEL_H */
