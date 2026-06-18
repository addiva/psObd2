/* it.massirito.pspobd2 - PSP OBD2 diagnostics & telemetry
 * (c) massirito. Homebrew non firmato crittograficamente: gira su CFW/HEN.
 * File: appconfig.c  |  Modulo: load/save configurazione app (JSON su ms0)
 */
#include "appconfig.h"
#include "json.h"
#include <string.h>
#include <stdio.h>

#ifdef PSP_BUILD
#include <pspiofilemgr.h>
#endif

void appconfig_defaults(app_config_t *cfg) {
    memset(cfg, 0, sizeof(*cfg));
    strncpy(cfg->elm_ip,  ELM_DEFAULT_IP, sizeof(cfg->elm_ip) - 1);
    cfg->elm_port        = ELM_DEFAULT_PORT;
    cfg->elm_apctl_index = ELM_DEFAULT_APCTL;
    strncpy(cfg->car_id, "mx5_nc", sizeof(cfg->car_id) - 1);
    cfg->last_eur_per_l  = 1.80f;
}

int appconfig_load(app_config_t *cfg, const char *path) {
    appconfig_defaults(cfg);
    FILE *f = fopen(path, "r");
    if (!f) return -1;

    char buf[2048];
    int n = (int)fread(buf, 1, sizeof(buf) - 1, f);
    fclose(f);
    buf[n] = '\0';

    json_node_t *root = json_parse(buf);
    if (!root) return -1;

    json_node_t *elm = json_get(root, "elm");
    if (elm) {
        const char *ip = json_str(elm, "ip", NULL);
        if (ip) strncpy(cfg->elm_ip, ip, sizeof(cfg->elm_ip) - 1);
        cfg->elm_port        = json_int(elm, "port",  cfg->elm_port);
        cfg->elm_apctl_index = json_int(elm, "apctl", cfg->elm_apctl_index);
    }

    const char *car = json_str(root, "car_id", NULL);
    if (car) strncpy(cfg->car_id, car, sizeof(cfg->car_id) - 1);

    cfg->last_eur_per_l = (float)json_num(root, "last_eur_l", cfg->last_eur_per_l);

    json_node_t *ab = json_get(root, "accel_best");
    if (ab && ab->type == JSON_ARRAY) {
        int i = 0;
        for (json_node_t *c = ab->v.children; c && i < 7; c = c->next, i++)
            if (c->type == JSON_NUMBER)
                cfg->accel_best[i] = (float)c->v.n;
    }

    json_free();
    return 0;
}

int appconfig_save(const app_config_t *cfg, const char *path) {
    char buf[2048];
    json_writer_t w;
    jw_init(&w, buf, sizeof(buf));
    jw_obj_open(&w);
    jw_key_obj_open(&w, "elm");
        jw_key_str(&w, "ip",    cfg->elm_ip);
        jw_key_int(&w, "port",  cfg->elm_port);
        jw_key_int(&w, "apctl", cfg->elm_apctl_index);
    jw_obj_close(&w);
    jw_key_str(&w, "car_id",      cfg->car_id);
    jw_key_dbl(&w, "last_eur_l",  cfg->last_eur_per_l);
    jw_key_arr_open(&w, "accel_best");
    for (int i = 0; i < 7; i++) {
        char tmp[32];
        snprintf(tmp, sizeof(tmp), "%.3f", cfg->accel_best[i]);
        /* Scrivi come numero raw senza virgolette */
        if (w.need_sep) { char *p = w.buf + w.len;
            if (w.len + 1 < w.cap) { *p = ','; w.len++; w.buf[w.len]='\0'; } }
        {
            int slen = (int)strlen(tmp);
            if (w.len + slen < w.cap) {
                memcpy(w.buf + w.len, tmp, slen);
                w.len += slen; w.buf[w.len]='\0';
            }
        }
        w.need_sep = 1;
    }
    jw_arr_close(&w);
    jw_obj_close(&w);

    if (!jw_ok(&w)) return -1;
    FILE *f = fopen(path, "w");
    if (!f) return -1;
    fputs(buf, f);
    fclose(f);
    return 0;
}

int appconfig_ensure_dir(const char *dir) {
#ifdef PSP_BUILD
    sceIoMkdir(dir, 0777);
    char snap[256];
    snprintf(snap, sizeof(snap), "%ssnapshots", dir);
    sceIoMkdir(snap, 0777);
#else
    /* Su host: non serve creare directory per i test */
    (void)dir;
#endif
    return 0;
}
