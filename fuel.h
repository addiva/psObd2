/* it.massirito.pspobd2 - PSP OBD2 diagnostics & telemetry
 * (c) massirito. Homebrew non firmato crittograficamente: gira su CFW/HEN.
 * File: fuel.h  |  Modulo: trip integrator, tank-to-tank, CSV log
 */
#ifndef FUEL_H
#define FUEL_H

/* Trip integrator: accumula consumo e distanza ad ogni campione OBD.
 * Tutti i valori SI (L, km, secondi). */
typedef struct {
    double sum_liters;   /* litri accumulati nel viaggio */
    double sum_km;       /* km percorsi nel viaggio */
    double last_dt_s;    /* ultimo intervallo di campionamento (s) */
} trip_t;

void trip_reset(trip_t *t);

/* Aggiorna l'integrale con un nuovo campione.
 * lh     = consumo istantaneo in L/h (da qualsiasi sorgente)
 * speed  = velocita' km/h
 * dt_s   = intervallo dal campione precedente in secondi */
void trip_update(trip_t *t, float lh, float speed, float dt_s);

/* Consumi medi sessione. Ritorna 0.0 se non ancora significativo. */
float trip_avg_l100(const trip_t *t);
float trip_avg_lh(const trip_t *t);

/* ---- Rifornimento (tank-to-tank) ---- */
typedef struct {
    char  date[20];      /* "YYYY-MM-DD HH:MM" */
    float km;            /* km percorsi dalla ricarica precedente */
    float liters;        /* litri erogati */
    float eur_per_l;     /* prezzo al litro */
    float l_per_100;     /* calcolato: liters/km*100 */
    float eur_per_100;   /* calcolato */
} refuel_t;

#define REFUEL_MAX 128

typedef struct {
    refuel_t entries[REFUEL_MAX];
    int      count;
} refuel_log_t;

void refuel_log_init(refuel_log_t *log);

/* Aggiunge un rifornimento e ricalcola i campi derivati.
 * Ritorna 0 OK, -1 log pieno. */
int  refuel_log_add(refuel_log_t *log, const char *date,
                    float km, float liters, float eur_per_l);

/* Media mobile L/100 e EUR/100 sugli ultimi N rifornimenti (0 = tutti) */
float refuel_avg_l100(const refuel_log_t *log, int last_n);
float refuel_avg_eur100(const refuel_log_t *log, int last_n);
float refuel_total_km(const refuel_log_t *log);
float refuel_total_eur(const refuel_log_t *log);

/* I/O CSV: "date,km,liters,eur_per_l,l_per_100,eur_per_100\n"
 * path = es. APP_DIR "fuel_mx5_nc.csv"
 * Ritorna 0 OK, -1 errore. */
int  refuel_load_csv(refuel_log_t *log, const char *path);
int  refuel_save_csv(const refuel_log_t *log, const char *path);

#endif /* FUEL_H */
