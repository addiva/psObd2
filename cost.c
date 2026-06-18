/* it.massirito.pspobd2 - PSP OBD2 diagnostics & telemetry
 * (c) massirito. Homebrew non firmato crittograficamente: gira su CFW/HEN.
 * File: cost.c  |  Modulo: cost engine (EUR/100km, EUR/L, storico)
 */
#include "cost.h"
#include <string.h>

void cost_compute(cost_summary_t *s, const refuel_log_t *log) {
    memset(s, 0, sizeof(*s));
    if (!log || log->count == 0) return;

    s->avg_l100_all    = refuel_avg_l100(log, 0);
    s->avg_l100_last5  = refuel_avg_l100(log, 5);
    s->avg_eur100_all  = refuel_avg_eur100(log, 0);
    s->avg_eur100_last5 = refuel_avg_eur100(log, 5);
    s->total_km        = refuel_total_km(log);
    s->total_eur       = refuel_total_eur(log);

    /* prezzo medio ponderato per litri */
    double sum_l = 0, sum_cost = 0;
    for (int i = 0; i < log->count; i++) {
        sum_l    += log->entries[i].liters;
        sum_cost += log->entries[i].liters * log->entries[i].eur_per_l;
    }
    s->avg_eur_l = (sum_l > 0) ? (float)(sum_cost / sum_l) : 0.0f;

    const refuel_t *last = &log->entries[log->count - 1];
    s->last_fill_eur  = last->liters * last->eur_per_l;
    s->last_fill_l100 = last->l_per_100;
}

/* Nota: sotto-stima in arricchimento (pieno gas) - AFR stechiometrico assunto */
float cost_l100_from_maf(float maf_gps, float speed_kmh,
                         float afr_stoich, float fuel_density_g_l) {
    if (maf_gps <= 0.0f || afr_stoich <= 0.0f || fuel_density_g_l <= 0.0f)
        return -1.0f;
    float lh = maf_gps * 3600.0f / (afr_stoich * fuel_density_g_l);
    if (speed_kmh < 1.0f) return -1.0f; /* mostra L/h, non /100 */
    return lh / speed_kmh * 100.0f;
}

float cost_l100_from_lh(float lh, float speed_kmh) {
    if (lh < 0.0f) return -1.0f;
    if (speed_kmh < 1.0f) return -1.0f;
    return lh / speed_kmh * 100.0f;
}
