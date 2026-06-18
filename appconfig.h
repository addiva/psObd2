/* it.massirito.pspobd2 - PSP OBD2 diagnostics & telemetry
 * (c) massirito. Homebrew non firmato crittograficamente: gira su CFW/HEN.
 * File: appconfig.h  |  Modulo: load/save configurazione app (JSON su ms0)
 */
#ifndef APPCONFIG_H
#define APPCONFIG_H

#include "config.h"

typedef struct {
    /* ELM327 */
    char elm_ip[64];
    int  elm_port;
    int  elm_apctl_index;

    /* Auto selezionata */
    char car_id[32];

    /* Accel best (serializzati per non perderli) */
    float accel_best[7];

    /* Ultimo costo al litro inserito (per pre-riempire il form) */
    float last_eur_per_l;
} app_config_t;

/* Imposta valori di default */
void appconfig_defaults(app_config_t *cfg);

/* Carica da CONFIG_FILE. Ritorna 0 OK, -1 errore (usa default). */
int  appconfig_load(app_config_t *cfg, const char *path);

/* Salva su path. Ritorna 0 OK, -1 errore. */
int  appconfig_save(const app_config_t *cfg, const char *path);

/* Crea la directory APP_DIR se non esiste (PSP: sceIoMkdir) */
int  appconfig_ensure_dir(const char *dir);

#endif /* APPCONFIG_H */
