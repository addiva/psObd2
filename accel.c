/* it.massirito.pspobd2 - PSP OBD2 diagnostics & telemetry
 * (c) massirito. Homebrew non firmato crittograficamente: gira su CFW/HEN.
 * File: accel.c  |  Modulo: cronometro accelerazione 0-20-50-100-120-150-200
 */
#include "accel.h"
#include <string.h>

const int accel_thresholds[ACCEL_THRESHOLDS_N] = { 0, 20, 50, 100, 120, 150, 200 };

void accel_reset(accel_run_t *r) {
    memset(r, 0, sizeof(*r));
}

void accel_start(accel_run_t *r, float now_s) {
    accel_reset(r);
    r->active      = 1;
    r->t_start_s   = now_s;
    r->prev_time_s = now_s;
    r->prev_speed  = 0;
    /* soglia 0 km/h (partenza) gia' raggiunta */
    r->hit[0]   = 1;
    r->t_hit[0] = 0.0f;
}

int accel_update(accel_run_t *r, int speed_kmh, float now_s) {
    if (!r->active) return 0;

    float dt = now_s - r->prev_time_s;
    int all_hit = 1;

    for (int i = 0; i < ACCEL_THRESHOLDS_N; i++) {
        if (r->hit[i]) continue;
        all_hit = 0;
        int thr = accel_thresholds[i];
        if (speed_kmh >= thr) {
            /* Interpolazione lineare tra campione precedente e corrente */
            float t_hit = r->prev_time_s;
            if (speed_kmh != r->prev_speed)
                t_hit = r->prev_time_s +
                        dt * (float)(thr - r->prev_speed) /
                        (float)(speed_kmh - r->prev_speed);
            r->t_hit[i] = t_hit - r->t_start_s;
            r->hit[i]   = 1;
        }
    }

    r->prev_speed  = speed_kmh;
    r->prev_time_s = now_s;

    /* Verifica se tutte le soglie sono state raggiunte */
    all_hit = 1;
    for (int i = 0; i < ACCEL_THRESHOLDS_N; i++)
        if (!r->hit[i]) { all_hit = 0; break; }

    if (all_hit) r->active = 0;
    return all_hit;
}

void accel_update_best(accel_best_t *best, const accel_run_t *r) {
    for (int i = 0; i < ACCEL_THRESHOLDS_N; i++) {
        if (!r->hit[i]) continue;
        if (best->best[i] == 0.0f || r->t_hit[i] < best->best[i])
            best->best[i] = r->t_hit[i];
    }
}
