/* it.massirito.pspobd2 - PSP OBD2 diagnostics & telemetry
 * (c) massirito. Homebrew non firmato crittograficamente: gira su CFW/HEN.
 * File: live.h  |  Modulo: metriche live condivise (thread OBD -> UI)
 */
#ifndef LIVE_H
#define LIVE_H

/* Sorgenti metriche (valori salvati in layout.json: aggiungere solo in coda) */
typedef enum {
    SRC_RPM = 0,
    SRC_SPEED,
    SRC_COOLANT,
    SRC_MAF,
    SRC_FUEL_LEVEL,
    SRC_THROTTLE,
    SRC_L100_LIVE,
    SRC_L100_TRIP,
    SRC_LH_LIVE,
    SRC_MIL,
    SRC_DTC_COUNT,
    SRC_FUEL_TOTAL_KM,
    SRC_FUEL_TOTAL_EUR,
    SRC_LAST_L100,
    SRC_LAST_EUR,
    SRC_AVG_L100,
    SRC_AVG_EUR100,
    SRC_ACCEL_TIME,
    SRC_BATT_V,
    SRC_TRIP_KM,
    SRC_KM_FILL,
    SRC_COUNT
} metric_src_t;

#define LIVE_BIT(src) (1u << (src))

typedef struct {
    float rpm;
    int   speed;
    int   coolant;
    float maf;
    float fuel_level;
    int   throttle;
    float lh_live;
    float l100_live;      /* -1 = auto ferma (vedi lh_live) */
    float l100_trip;
    float trip_km;
    float batt_v;
    int   mil;
    int   dtc_ecu;        /* numero DTC dichiarato dalla ECU (PID 0101) */
    unsigned int valid;   /* LIVE_BIT(src) = valore disponibile */
} live_t;

#endif /* LIVE_H */
