/* it.massirito.pspobd2 - PSP OBD2 diagnostics & telemetry
 * (c) massirito. Homebrew non firmato crittograficamente: gira su CFW/HEN.
 * File: profile.c  |  Modulo: registry profili auto e capability
 */
#include "profile.h"
#include <string.h>

/* ---- Registry profili ---- */

const car_profile_t profile_registry[PROFILE_COUNT] = {
    {
        .id                   = "mx5_nc",
        .display_name         = "Mazda MX-5 NC 2010",
        .fuel                 = FUEL_PETROL,
        .fuel_density         = 745.0f,
        .afr_stoich           = 14.7f,
        .tank_liters          = 50.0f,
        .direct_consumption   = 0,
        .direct_consumption_pid = 0,
        .prefer_fuel_rate_015e = 0,
        .notes = "Nessun consumo diretto. Usa calcolo MAF se 0110 disponibile, "
                 "poi livello 012F per stima tratta. Accel timer attivo."
    },
    {
        .id                   = "c2_hdi",
        .display_name         = "Citroen C2 1.4 HDi 2005",
        .fuel                 = FUEL_DIESEL,
        .fuel_density         = 835.0f,
        .afr_stoich           = 14.5f,
        .tank_liters          = 44.0f,
        .direct_consumption   = 0,
        .direct_consumption_pid = 0,
        .prefer_fuel_rate_015e = 1,
        .notes = "Live data scarso su PSA diesels. DTC OK. "
                 "Consumo live solo se 015E risponde, altrimenti tank-to-tank."
    },
    {
        .id                   = "bmw_f30",
        .display_name         = "BMW F30 (generica)",
        .fuel                 = FUEL_PETROL,
        .fuel_density         = 745.0f,
        .afr_stoich           = 14.7f,
        .tank_liters          = 60.0f,
        .direct_consumption   = 0,
        .direct_consumption_pid = 0,
        .prefer_fuel_rate_015e = 1,
        .notes = "AVVISO: legge solo DTC powertrain standard (Mode 03). "
                 "Centraline non-motore e codici P1xxx BMW richiedono software "
                 "proprietario. Se 015E risponde, consumo live disponibile."
    }
};

const car_profile_t *profile_find(const char *id) {
    if (!id) return NULL;
    for (int i = 0; i < PROFILE_COUNT; i++)
        if (strcmp(profile_registry[i].id, id) == 0)
            return &profile_registry[i];
    return NULL;
}

car_caps_t caps_empty(void) {
    car_caps_t c;
    memset(&c, 0, sizeof(c));
    c.cons = CONS_NONE;
    return c;
}

/* Risolutore strategia consumo (§4 del master prompt):
 * 1. direct_consumption + PID custom risponde
 * 2. 015E fuel rate disponibile
 * 3. benzina + MAF disponibile -> calcolo MAF
 * 4. fallback: niente live */
void profile_resolve_strategy(const car_profile_t *p, car_caps_t *caps) {
    if (!p || !caps) return;
    if (p->direct_consumption && caps->pid_fuel_rate /* usa il flag generico */) {
        caps->cons = CONS_DIRECT_PID;
        return;
    }
    if (p->prefer_fuel_rate_015e && caps->pid_fuel_rate) {
        caps->cons = CONS_FUEL_RATE;
        return;
    }
    if (caps->pid_fuel_rate) {
        caps->cons = CONS_FUEL_RATE;
        return;
    }
    if (p->fuel == FUEL_PETROL && caps->pid_maf) {
        caps->cons = CONS_MAF;
        return;
    }
    caps->cons = CONS_NONE;
}
