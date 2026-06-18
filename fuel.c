/* it.massirito.pspobd2 - PSP OBD2 diagnostics & telemetry
 * (c) massirito. Homebrew non firmato crittograficamente: gira su CFW/HEN.
 * File: fuel.c  |  Modulo: trip integrator, tank-to-tank, CSV log
 */
#include "fuel.h"
#include <string.h>
#include <stdio.h>

/* ---- Trip integrator ---- */

void trip_reset(trip_t *t) {
    memset(t, 0, sizeof(*t));
}

void trip_update(trip_t *t, float lh, float speed, float dt_s) {
    if (dt_s <= 0.0f || lh < 0.0f || speed < 0.0f) return;
    double h = dt_s / 3600.0;
    t->sum_liters += lh * h;
    t->sum_km     += speed * h;
    t->last_dt_s   = dt_s;
}

float trip_avg_l100(const trip_t *t) {
    if (t->sum_km < 0.01) return 0.0f;
    return (float)(t->sum_liters / t->sum_km * 100.0);
}

float trip_avg_lh(const trip_t *t) {
    /* media integrata nel tempo: sum_liters / tempo_totale */
    double h_tot = t->sum_km > 0 ? t->sum_km / 60.0 : 0;
    if (h_tot < 1e-6) return 0.0f;
    return (float)(t->sum_liters / h_tot);
}

/* ---- Rifornimento ---- */

void refuel_log_init(refuel_log_t *log) {
    memset(log, 0, sizeof(*log));
}

int refuel_log_add(refuel_log_t *log, const char *date,
                   float km, float liters, float eur_per_l) {
    if (log->count >= REFUEL_MAX) return -1;
    refuel_t *r = &log->entries[log->count++];
    strncpy(r->date, date ? date : "", sizeof(r->date) - 1);
    r->km         = km;
    r->liters     = liters;
    r->eur_per_l  = eur_per_l;
    r->l_per_100  = (km > 0.0f) ? (liters / km * 100.0f) : 0.0f;
    r->eur_per_100 = r->l_per_100 * eur_per_l;
    return 0;
}

float refuel_avg_l100(const refuel_log_t *log, int last_n) {
    if (log->count == 0) return 0.0f;
    int start = 0;
    if (last_n > 0 && last_n < log->count) start = log->count - last_n;
    double sum_l = 0, sum_km = 0;
    for (int i = start; i < log->count; i++) {
        sum_l  += log->entries[i].liters;
        sum_km += log->entries[i].km;
    }
    return sum_km > 0 ? (float)(sum_l / sum_km * 100.0) : 0.0f;
}

float refuel_avg_eur100(const refuel_log_t *log, int last_n) {
    if (log->count == 0) return 0.0f;
    int start = 0;
    if (last_n > 0 && last_n < log->count) start = log->count - last_n;
    double sum_e = 0, sum_km = 0;
    for (int i = start; i < log->count; i++) {
        sum_e  += log->entries[i].liters * log->entries[i].eur_per_l;
        sum_km += log->entries[i].km;
    }
    return sum_km > 0 ? (float)(sum_e / sum_km * 100.0) : 0.0f;
}

float refuel_total_km(const refuel_log_t *log) {
    float tot = 0;
    for (int i = 0; i < log->count; i++) tot += log->entries[i].km;
    return tot;
}

float refuel_total_eur(const refuel_log_t *log) {
    float tot = 0;
    for (int i = 0; i < log->count; i++)
        tot += log->entries[i].liters * log->entries[i].eur_per_l;
    return tot;
}

int refuel_load_csv(refuel_log_t *log, const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) return -1;
    char line[256];
    /* salta header */
    if (!fgets(line, sizeof(line), f)) { fclose(f); return 0; }
    while (fgets(line, sizeof(line), f) && log->count < REFUEL_MAX) {
        char date[20] = "";
        float km = 0, lit = 0, epl = 0;
        /* formato: date,km,liters,eur_per_l,l_per_100,eur_per_100 */
        if (sscanf(line, "%19[^,],%f,%f,%f", date, &km, &lit, &epl) == 4)
            refuel_log_add(log, date, km, lit, epl);
    }
    fclose(f);
    return 0;
}

int refuel_save_csv(const refuel_log_t *log, const char *path) {
    FILE *f = fopen(path, "w");
    if (!f) return -1;
    fprintf(f, "date,km,liters,eur_per_l,l_per_100,eur_per_100\n");
    for (int i = 0; i < log->count; i++) {
        const refuel_t *r = &log->entries[i];
        fprintf(f, "%s,%.1f,%.3f,%.4f,%.2f,%.2f\n",
                r->date, r->km, r->liters, r->eur_per_l,
                r->l_per_100, r->eur_per_100);
    }
    fclose(f);
    return 0;
}
