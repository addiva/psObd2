/* it.massirito.pspobd2 - PSP OBD2 diagnostics & telemetry
 * (c) massirito. Homebrew non firmato crittograficamente: gira su CFW/HEN.
 * File: profile.h  |  Modulo: registry profili auto e capability
 */
#ifndef PROFILE_H
#define PROFILE_H

typedef enum { FUEL_PETROL, FUEL_DIESEL } fuel_type_t;

/* Strategia consumo selezionata dopo capability check a runtime */
typedef enum {
    CONS_NONE,         /* niente live, solo tank-to-tank */
    CONS_DIRECT_PID,   /* PID custom dell'auto espone consumo diretto */
    CONS_FUEL_RATE,    /* PID 015E fuel rate L/h */
    CONS_MAF           /* calcolo da MAF (benzina) */
} cons_strategy_t;

/* Capability bitmap aggiornata a runtime dopo query 0100/0120/0140/0160 */
typedef struct {
    int pid_rpm;           /* 010C */
    int pid_speed;         /* 010D */
    int pid_coolant;       /* 0105 */
    int pid_maf;           /* 0110 */
    int pid_fuel_level;    /* 012F */
    int pid_throttle;      /* 0111 */
    int pid_fuel_rate;     /* 015E */
    int pid_mil;           /* 0101 */
    cons_strategy_t cons;
} car_caps_t;

typedef struct {
    const char   *id;               /* "mx5_nc", "c2_hdi", "bmw_f30" */
    const char   *display_name;     /* testo mostrato in UI */
    fuel_type_t   fuel;
    float         fuel_density;     /* g/L (benzina ~745, diesel ~835) */
    float         afr_stoich;       /* benzina 14.7, diesel ~14.5 */
    float         tank_liters;
    int           direct_consumption;     /* 1 se espone PID consumo custom */
    int           direct_consumption_pid; /* PID custom (hex) */
    int           prefer_fuel_rate_015e;  /* 1 = tenta 015E prima */
    const char   *notes;            /* caveat/note specifiche */
} car_profile_t;

/* Numero di profili registrati */
#define PROFILE_COUNT 3

/* Registry globale */
extern const car_profile_t profile_registry[PROFILE_COUNT];

/* Cerca per id. Ritorna NULL se non trovato. */
const car_profile_t *profile_find(const char *id);

/* Determina la strategia di consumo basandosi sul profilo e le capability
 * rilevate a runtime. Aggiorna caps->cons in place. */
void profile_resolve_strategy(const car_profile_t *p, car_caps_t *caps);

/* Crea una car_caps_t vuota (tutto non supportato) */
car_caps_t caps_empty(void);

#endif /* PROFILE_H */
