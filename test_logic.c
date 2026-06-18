/* it.massirito.pspobd2 - PSP OBD2 diagnostics & telemetry
 * (c) massirito. Homebrew non firmato crittograficamente: gira su CFW/HEN.
 * File: test_logic.c  |  Modulo: harness host gcc per obd/fuel/cost/accel/json/dtc_db
 *
 * Compila con:
 *   gcc -Wall -Wextra -o test_logic test_logic.c obd.c fuel.c cost.c accel.c \
 *       json.c appconfig.c profile.c dtc_db.c -lm && ./test_logic
 *
 * Non include nessun header PSP-specifico.
 */
#include <stdio.h>
#include <math.h>
#include <string.h>
#include <assert.h>

#include "obd.h"
#include "fuel.h"
#include "cost.h"
#include "accel.h"
#include "json.h"
#include "profile.h"
#include "appconfig.h"
#include "dtc_db.h"

static int s_pass = 0, s_fail = 0;

#define CHECK(label, cond) \
    do { if (cond) { printf("  PASS: %s\n", label); s_pass++; } \
         else      { printf("  FAIL: %s\n", label); s_fail++; } } while(0)

#define CHECK_FLOAT(label, a, b, tol) \
    CHECK(label, fabsf((a) - (b)) < (tol))

/* ---- OBD parsing ---- */
static void test_obd(void) {
    printf("=== OBD parsing ===\n");
    uint8_t d[4];

    /* 010C 1A F8 -> (0x1A*256+0xF8)/4 = (26*256+248)/4 = 6904/4 = 1726 rpm */
    d[0]=0x1A; d[1]=0xF8;
    CHECK_FLOAT("RPM 010C", obd_rpm(d,2), 1726.0f, 0.1f);

    /* 010C 07 E4 -> (0x07*256+0xE4)/4 = (1792+228)/4 = 2020/4 = 505 rpm */
    d[0]=0x07; d[1]=0xE4;
    CHECK_FLOAT("RPM low", obd_rpm(d,2), 505.0f, 0.1f);

    /* 010D 40 -> 64 km/h */
    d[0]=0x40;
    CHECK("Speed", obd_speed(d,1) == 64);

    /* 0105 7B -> 0x7B-40 = 123-40 = 83 C */
    d[0]=0x7B;
    CHECK("Coolant", obd_coolant_c(d,1) == 83);

    /* 0110 07 80 -> (7*256+128)/100 = (1792+128)/100 = 19.2 g/s */
    d[0]=0x07; d[1]=0x80;
    CHECK_FLOAT("MAF", obd_maf_gps(d,2), 19.2f, 0.01f);

    /* 012F 80 -> 128*100/255 = 50.2% */
    d[0]=0x80;
    CHECK_FLOAT("Fuel level", obd_fuel_level_pct(d,1), 50.196f, 0.01f);

    /* 0111 80 -> 128*100/255 = 50% */
    d[0]=0x80;
    CHECK("Throttle", obd_throttle_pct(d,1) == 50);

    /* 015E 00 C8 -> (0*256+200)/20 = 10.0 L/h */
    d[0]=0x00; d[1]=0xC8;
    CHECK_FLOAT("Fuel rate", obd_fuel_rate_lh(d,2), 10.0f, 0.01f);

    /* MIL: 0101 byte A = 0x81 -> MIL on, 1 DTC */
    d[0]=0x81;
    int mil;
    int ndtc = obd_mil_status(d, 1, &mil);
    CHECK("MIL on", mil == 1);
    CHECK("nDTC=1", ndtc == 1);

    /* DTC decodifica: 0x03 0x01 -> P0301 */
    char dtc[6];
    obd_decode_dtc(0x03, 0x01, dtc);
    CHECK("DTC P0301", strcmp(dtc, "P0301") == 0);

    /* DTC: 0x43 0x12 -> C4312? No: letter=C (bit7-6=01), d1=(4>>4)&3=0, d2=4&F=4,
     * wait: a=0x43 -> (0x43>>6)&3 = 1 -> 'C', d1=(0x43>>4)&3=0, d2=0x43&0xF=3
     * b=0x12 -> d3=(0x12>>4)=1, d4=0x12&0xF=2 -> C0312 */
    obd_decode_dtc(0x43, 0x12, dtc);
    CHECK("DTC C0312", strcmp(dtc, "C0312") == 0);

    /* obd_parse_response */
    uint8_t out[8];
    int n = obd_parse_response("41 0C 1A F8\r\n>", out, sizeof(out));
    CHECK("parse_response count", n == 4);
    CHECK("parse_response b0", out[0] == 0x41);
    CHECK("parse_response b2", out[2] == 0x1A);

    /* Test con NO DATA */
    n = obd_parse_response("NO DATA\r\n>", out, sizeof(out));
    CHECK("parse_response NODATA", n == 0);
}

/* ---- Fuel & trip ---- */
static void test_fuel(void) {
    printf("=== Fuel & trip ===\n");

    trip_t t;
    trip_reset(&t);
    /* 5 L/h a 100 km/h per 1 ora = 5L / 100km -> 5 L/100 */
    trip_update(&t, 5.0f, 100.0f, 3600.0f);
    CHECK_FLOAT("Trip L/100", trip_avg_l100(&t), 5.0f, 0.01f);

    /* Aggiunge altri 10L/h a 50 km/h per 1 ora:
     * totale: 15L / 150km -> 10 L/100 */
    trip_update(&t, 10.0f, 50.0f, 3600.0f);
    CHECK_FLOAT("Trip L/100 acc", trip_avg_l100(&t), 10.0f, 0.01f);

    refuel_log_t log;
    refuel_log_init(&log);
    refuel_log_add(&log, "2024-01-01 10:00", 500.0f, 40.0f, 1.80f);
    refuel_log_add(&log, "2024-01-15 10:00", 600.0f, 45.0f, 1.85f);

    /* L/100 = liters/km*100 */
    float avg = refuel_avg_l100(&log, 0);
    float expected = (40.0f + 45.0f) / (500.0f + 600.0f) * 100.0f;
    CHECK_FLOAT("Refuel avg L/100", avg, expected, 0.01f);

    CHECK_FLOAT("Total km",  refuel_total_km(&log),  1100.0f, 0.1f);
    float total_eur = 40*1.80f + 45*1.85f;
    CHECK_FLOAT("Total EUR", refuel_total_eur(&log), total_eur, 0.01f);
}

/* ---- Cost engine ---- */
static void test_cost(void) {
    printf("=== Cost engine ===\n");

    /* MAF: 19.2 g/s, 64 km/h, benzina (afr=14.7, density=745)
     * L/h = 19.2 * 3600 / (14.7 * 745) = 69120 / 10951.5 = 6.31 L/h
     * L/100 = 6.31 / 64 * 100 = 9.86 */
    float l100 = cost_l100_from_maf(19.2f, 64.0f, 14.7f, 745.0f);
    CHECK_FLOAT("L/100 from MAF", l100, 9.86f, 0.1f);

    /* fuel rate: 10 L/h a 100 km/h -> 10 L/100 */
    float l100b = cost_l100_from_lh(10.0f, 100.0f);
    CHECK_FLOAT("L/100 from LH",  l100b, 10.0f, 0.01f);

    /* speed=0 -> -1 (non definito) */
    float l100c = cost_l100_from_lh(5.0f, 0.0f);
    CHECK("L/100 speed0 undef", l100c < 0);

    refuel_log_t log;
    refuel_log_init(&log);
    refuel_log_add(&log, "2024-01-01", 400, 35, 1.80f);
    refuel_log_add(&log, "2024-01-15", 500, 45, 1.85f);
    cost_summary_t s;
    cost_compute(&s, &log);
    CHECK_FLOAT("cost total km", s.total_km, 900.0f, 0.1f);
    float total_eur = 35*1.80f + 45*1.85f;
    CHECK_FLOAT("cost total eur", s.total_eur, total_eur, 0.01f);
    /* L/100 media: (35+45)/(400+500)*100 = 80/900*100 = 8.89 */
    CHECK_FLOAT("cost avg l100", s.avg_l100_all, 80.0f/900.0f*100.0f, 0.1f);
}

/* ---- Accel timer ---- */
static void test_accel(void) {
    printf("=== Accel timer ===\n");

    accel_run_t r;
    accel_reset(&r);
    accel_start(&r, 0.0f);
    CHECK("Accel active", r.active == 1);
    CHECK("Soglia 0 hit", r.hit[0] == 1);

    /* Simulazione: velocita' sale linearmente 0->200 km/h in 10s */
    for (int i = 0; i <= 100; i++) {
        float t = i * 0.1f;
        int v   = (int)(i * 2);   /* 0..200 km/h */
        if (accel_update(&r, v, t)) break;
    }

    CHECK("Hit 20",  r.hit[1] == 1);
    CHECK("Hit 50",  r.hit[2] == 1);
    CHECK("Hit 100", r.hit[3] == 1);
    CHECK("Hit 120", r.hit[4] == 1);
    CHECK("Hit 150", r.hit[5] == 1);
    CHECK("Hit 200", r.hit[6] == 1);

    /* A 200 km/h in 10s lineari: t_hit(100) = 5s */
    CHECK_FLOAT("t_hit 100 km/h", r.t_hit[3], 5.0f, 0.05f);

    accel_best_t best;
    memset(&best, 0, sizeof(best));
    accel_update_best(&best, &r);
    CHECK("Best 0-100 set", best.best[3] > 0.0f);
    CHECK_FLOAT("Best 0-100 val", best.best[3], 5.0f, 0.05f);
}

/* ---- JSON parser/writer ---- */
static void test_json(void) {
    printf("=== JSON ===\n");

    const char *src = "{\"elm\":{\"ip\":\"192.168.0.10\",\"port\":35000},"
                      "\"car_id\":\"mx5_nc\",\"last_eur_l\":1.85}";
    json_node_t *root = json_parse(src);
    CHECK("parse ok",    root != NULL);
    CHECK("root object", root && root->type == JSON_OBJECT);

    const char *car = json_str(root, "car_id", "");
    CHECK("car_id",      strcmp(car, "mx5_nc") == 0);

    double eur = json_num(root, "last_eur_l", 0);
    CHECK_FLOAT("last_eur_l", (float)eur, 1.85f, 0.001f);

    json_node_t *elm = json_get(root, "elm");
    CHECK("elm obj",  elm && elm->type == JSON_OBJECT);
    int port = json_int(elm, "port", 0);
    CHECK("port",     port == 35000);
    const char *ip = json_str(elm, "ip", "");
    CHECK("ip",       strcmp(ip, "192.168.0.10") == 0);

    json_free();

    /* Writer */
    char buf[512];
    json_writer_t w;
    jw_init(&w, buf, sizeof(buf));
    jw_obj_open(&w);
    jw_key_str(&w, "car_id", "mx5_nc");
    jw_key_int(&w, "port", 35000);
    jw_key_dbl(&w, "price", 1.85);
    jw_obj_close(&w);
    CHECK("writer ok",    jw_ok(&w));
    CHECK("writer car_id", strstr(buf, "\"mx5_nc\"") != NULL);
    CHECK("writer port",   strstr(buf, "35000") != NULL);
}

/* ---- Profile registry ---- */
static void test_profile(void) {
    printf("=== Profile registry ===\n");

    const car_profile_t *mx5 = profile_find("mx5_nc");
    CHECK("mx5_nc found",   mx5 != NULL);
    CHECK("mx5 petrol",     mx5 && mx5->fuel == FUEL_PETROL);
    CHECK_FLOAT("mx5 tank", mx5 ? mx5->tank_liters : 0, 50.0f, 0.1f);
    CHECK("mx5 no direct",  mx5 && mx5->direct_consumption == 0);

    const car_profile_t *c2 = profile_find("c2_hdi");
    CHECK("c2_hdi found",   c2 != NULL);
    CHECK("c2 diesel",      c2 && c2->fuel == FUEL_DIESEL);

    const car_profile_t *bmw = profile_find("bmw_f30");
    CHECK("bmw_f30 found",  bmw != NULL);

    CHECK("not found",      profile_find("nonexistent") == NULL);

    /* Capability resolve */
    car_caps_t caps = caps_empty();
    caps.pid_maf = 1;
    profile_resolve_strategy(mx5, &caps);
    CHECK("MX5 strategy MAF", caps.cons == CONS_MAF);

    caps = caps_empty();
    caps.pid_fuel_rate = 1;
    profile_resolve_strategy(c2, &caps);
    CHECK("C2 strategy FUEL_RATE", caps.cons == CONS_FUEL_RATE);

    caps = caps_empty();
    profile_resolve_strategy(c2, &caps);
    CHECK("C2 no cap -> NONE", caps.cons == CONS_NONE);
}

/* ---- appconfig ---- */
static void test_appconfig(void) {
    printf("=== AppConfig ===\n");

    app_config_t cfg;
    appconfig_defaults(&cfg);
    CHECK("default ip",   strcmp(cfg.elm_ip, ELM_DEFAULT_IP) == 0);
    CHECK("default port", cfg.elm_port == ELM_DEFAULT_PORT);
    CHECK("default car",  strcmp(cfg.car_id, "mx5_nc") == 0);

    /* Save + reload */
    const char *tmp = "/tmp/pspobd2_test_cfg.json";
    int r = appconfig_save(&cfg, tmp);
    CHECK("save ok", r == 0);

    app_config_t cfg2;
    memset(&cfg2, 0, sizeof(cfg2));
    r = appconfig_load(&cfg2, tmp);
    CHECK("load ok",    r == 0);
    CHECK("load ip",    strcmp(cfg2.elm_ip, ELM_DEFAULT_IP) == 0);
    CHECK("load port",  cfg2.elm_port == ELM_DEFAULT_PORT);
    CHECK("load car",   strcmp(cfg2.car_id, "mx5_nc") == 0);
}

/* ---- MAF formula dal master prompt ---- */
static void test_maf_formula(void) {
    printf("=== MAF formula (§4 master prompt) ===\n");
    /* 010C 1A F8 -> 1726 rpm (nota: con d={0x1A,0xF8}: 0x1A=26, 0xF8=248 -> 26*256+248=6904 /4=1726) */
    uint8_t d[2] = {0x1A, 0xF8};
    CHECK_FLOAT("RPM 1726", obd_rpm(d,2), 1726.0f, 0.1f);

    /* 0110 07 80 -> 19.2 g/s */
    d[0]=0x07; d[1]=0x80;
    float maf = obd_maf_gps(d,2);
    CHECK_FLOAT("MAF 19.2", maf, 19.2f, 0.01f);

    /* L/h = 19.2 * 3600 / (14.7 * 745) = 6.313 L/h */
    float lh = maf * 3600.0f / (14.7f * 745.0f);
    CHECK_FLOAT("L/h from MAF", lh, 6.313f, 0.01f);

    /* A 64 km/h: L/100 = 6.313/64*100 = 9.86 */
    float l100 = cost_l100_from_maf(maf, 64.0f, 14.7f, 745.0f);
    CHECK_FLOAT("L/100 at 64kmh", l100, 9.86f, 0.1f);
}

/* ---- DTC database ---- */
static void test_dtc_db(void) {
    printf("=== DTC database ===\n");

    /* Database non vuoto */
    CHECK("db non vuoto", dtc_db_count > 0);

    /* Codici generici standard */
    CHECK("P0301 trovato", dtc_lookup("P0301") != NULL);
    CHECK("P0171 trovato", dtc_lookup("P0171") != NULL);
    CHECK("P0420 trovato", dtc_lookup("P0420") != NULL);
    CHECK("P0011 trovato", dtc_lookup("P0011") != NULL);

    /* Codici Mazda MX-5 NC specifici */
    CHECK("P2004 IMRC",    dtc_lookup("P2004") != NULL);
    CHECK("P2006 IMRC",    dtc_lookup("P2006") != NULL);
    CHECK("P1260 imm.",    dtc_lookup("P1260") != NULL);
    CHECK("P1170 O2",      dtc_lookup("P1170") != NULL);

    /* Contenuto descrizioni */
    const char *d301 = dtc_lookup("P0301");
    CHECK("P0301 desc contiene cil.1",
          d301 && strstr(d301, "cilindro 1") != NULL);

    const char *d2004 = dtc_lookup("P2004");
    CHECK("P2004 desc contiene IMRC",
          d2004 && strstr(d2004, "IMRC") != NULL);

    const char *dvvt = dtc_lookup("P0011");
    CHECK("P0011 desc contiene VVT",
          dvvt && strstr(dvvt, "VVT") != NULL);

    /* Lookup case-insensitive */
    CHECK("lookup lowercase", dtc_lookup("p0301") != NULL);
    CHECK("lookup spazi",     dtc_lookup("P 03 01") != NULL);

    /* Codice non esistente */
    CHECK("non esistente",    dtc_lookup("P9999") == NULL);
    CHECK("NULL safe",        dtc_lookup(NULL) == NULL);

    /* is_manufacturer */
    CHECK("P1xxx = mfr",      dtc_is_manufacturer("P1260") == 1);
    CHECK("P2xxx != mfr",     dtc_is_manufacturer("P2004") == 0);
    CHECK("P0xxx != mfr",     dtc_is_manufacturer("P0301") == 0);

    /* Tutti i codici hanno codice e descrizione non NULL */
    int all_ok = 1;
    for (int i = 0; i < dtc_db_count; i++) {
        if (!dtc_db[i].code || !dtc_db[i].description) { all_ok = 0; break; }
        if (dtc_db[i].code[0] == '\0' || dtc_db[i].description[0] == '\0')
            { all_ok = 0; break; }
    }
    CHECK("tutti i record validi", all_ok);

    /* Stampa quanti codici MX-5 NC ci sono */
    int nc_count = 0;
    for (int i = 0; i < dtc_db_count; i++)
        if (dtc_db[i].mx5_nc) nc_count++;
    printf("  Info: %d codici totali, %d specifici/rilevanti MX-5 NC\n",
           dtc_db_count, nc_count);
}

int main(void) {
    printf("=== psOBD2 test_logic ===\n\n");
    test_obd();
    test_fuel();
    test_cost();
    test_accel();
    test_json();
    test_profile();
    test_appconfig();
    test_maf_formula();
    test_dtc_db();

    printf("\n=== RISULTATO: %d PASS, %d FAIL ===\n", s_pass, s_fail);
    return s_fail > 0 ? 1 : 0;
}
