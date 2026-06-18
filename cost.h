/* it.massirito.pspobd2 - PSP OBD2 diagnostics & telemetry
 * (c) massirito. Homebrew non firmato crittograficamente: gira su CFW/HEN.
 * File: cost.h  |  Modulo: cost engine (EUR/100km, EUR/L, storico)
 */
#ifndef COST_H
#define COST_H

#include "fuel.h"

/* Snapshot costi calcolati, aggiornato su richiesta */
typedef struct {
    float avg_l100_all;       /* media globale L/100km (tank-to-tank) */
    float avg_l100_last5;     /* ultimi 5 rifornimenti */
    float avg_eur100_all;     /* EUR/100km globale */
    float avg_eur100_last5;
    float avg_eur_l;          /* prezzo medio al litro su tutti i rifornimenti */
    float total_km;
    float total_eur;
    float last_fill_eur;      /* spesa ultimo rifornimento */
    float last_fill_l100;     /* consumo ultimo rifornimento */
} cost_summary_t;

/* Ricalcola il summary dal log. */
void cost_compute(cost_summary_t *s, const refuel_log_t *log);

/* Calcolo consumo istantaneo in L/100 da strategia MAF */
float cost_l100_from_maf(float maf_gps, float speed_kmh,
                         float afr_stoich, float fuel_density_g_l);

/* Calcolo consumo istantaneo in L/100 da fuel rate */
float cost_l100_from_lh(float lh, float speed_kmh);

#endif /* COST_H */
