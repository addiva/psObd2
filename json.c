/* it.massirito.pspobd2 - PSP OBD2 diagnostics & telemetry
 * (c) massirito. Homebrew non firmato crittograficamente: gira su CFW/HEN.
 * File: json.c  |  Modulo: parser JSON minimale (config/layout su ms0)
 */
#include "json.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>

/* Pool fisso per evitare frammentazione heap su PSP */
#define POOL_NODES  512
#define POOL_STRS   8192

static json_node_t g_nodes[POOL_NODES];
static char        g_strs[POOL_STRS];
static int         g_ni;   /* indice prossimo nodo */
static int         g_si;   /* indice prossimo char nel pool stringhe */

static json_node_t *node_alloc(void) {
    if (g_ni >= POOL_NODES) return NULL;
    json_node_t *n = &g_nodes[g_ni++];
    memset(n, 0, sizeof(*n));
    return n;
}

static char *str_intern(const char *s, int len) {
    if (g_si + len + 1 > POOL_STRS) return NULL;
    char *p = &g_strs[g_si];
    memcpy(p, s, len);
    p[len] = '\0';
    g_si += len + 1;
    return p;
}

/* ---- Parser ---- */
typedef struct { const char *p; const char *end; } P;

static void skip_ws(P *p) {
    while (p->p < p->end && isspace((unsigned char)*p->p)) p->p++;
}

static json_node_t *parse_value(P *p);

static json_node_t *parse_string_raw(P *p, const char **out_s, int *out_len) {
    /* p->p punta al '"' di apertura */
    p->p++;
    const char *start = p->p;
    while (p->p < p->end && *p->p != '"') {
        if (*p->p == '\\') p->p++; /* salta il char escapato */
        p->p++;
    }
    *out_s = start;
    *out_len = (int)(p->p - start);
    if (p->p < p->end) p->p++; /* salta '"' chiusura */
    return (json_node_t *)1;   /* sentinella: ok */
}

static json_node_t *parse_string(P *p) {
    const char *s; int len;
    if (!parse_string_raw(p, &s, &len)) return NULL;
    json_node_t *n = node_alloc();
    if (!n) return NULL;
    n->type = JSON_STRING;
    n->v.s = str_intern(s, len);
    return n;
}

static json_node_t *parse_number(P *p) {
    char buf[64]; int i = 0;
    if (*p->p == '-') buf[i++] = *p->p++;
    while (p->p < p->end && i < 63 &&
           (isdigit((unsigned char)*p->p) || *p->p == '.' ||
            *p->p == 'e' || *p->p == 'E' || *p->p == '+' || *p->p == '-'))
        buf[i++] = *p->p++;
    buf[i] = '\0';
    json_node_t *n = node_alloc();
    if (!n) return NULL;
    n->type = JSON_NUMBER;
    n->v.n = atof(buf);
    return n;
}

static json_node_t *parse_object(P *p) {
    p->p++; /* salta '{' */
    json_node_t *obj = node_alloc();
    if (!obj) return NULL;
    obj->type = JSON_OBJECT;
    json_node_t *last = NULL;
    for (;;) {
        skip_ws(p);
        if (p->p >= p->end || *p->p == '}') break;
        if (*p->p == ',') { p->p++; continue; }
        if (*p->p != '"') return NULL;
        const char *ks; int klen;
        parse_string_raw(p, &ks, &klen);
        char *key = str_intern(ks, klen);
        skip_ws(p);
        if (p->p >= p->end || *p->p != ':') return NULL;
        p->p++;
        skip_ws(p);
        json_node_t *val = parse_value(p);
        if (!val) return NULL;
        val->key = key;
        if (!last) obj->v.children = val;
        else last->next = val;
        last = val;
    }
    if (p->p < p->end) p->p++; /* salta '}' */
    return obj;
}

static json_node_t *parse_array(P *p) {
    p->p++; /* salta '[' */
    json_node_t *arr = node_alloc();
    if (!arr) return NULL;
    arr->type = JSON_ARRAY;
    json_node_t *last = NULL;
    for (;;) {
        skip_ws(p);
        if (p->p >= p->end || *p->p == ']') break;
        if (*p->p == ',') { p->p++; continue; }
        json_node_t *val = parse_value(p);
        if (!val) return NULL;
        if (!last) arr->v.children = val;
        else last->next = val;
        last = val;
    }
    if (p->p < p->end) p->p++; /* salta ']' */
    return arr;
}

static json_node_t *parse_value(P *p) {
    skip_ws(p);
    if (p->p >= p->end) return NULL;
    char c = *p->p;
    if (c == '"') return parse_string(p);
    if (c == '{') return parse_object(p);
    if (c == '[') return parse_array(p);
    if (c == 'n' && p->end - p->p >= 4 && memcmp(p->p, "null", 4) == 0) {
        p->p += 4;
        json_node_t *n = node_alloc();
        if (n) n->type = JSON_NULL;
        return n;
    }
    if (c == 't' && p->end - p->p >= 4 && memcmp(p->p, "true", 4) == 0) {
        p->p += 4;
        json_node_t *n = node_alloc();
        if (n) { n->type = JSON_BOOL; n->v.b = 1; }
        return n;
    }
    if (c == 'f' && p->end - p->p >= 5 && memcmp(p->p, "false", 5) == 0) {
        p->p += 5;
        json_node_t *n = node_alloc();
        if (n) { n->type = JSON_BOOL; n->v.b = 0; }
        return n;
    }
    if (c == '-' || isdigit((unsigned char)c)) return parse_number(p);
    return NULL;
}

json_node_t *json_parse(const char *text) {
    g_ni = 0; g_si = 0;
    if (!text) return NULL;
    P p = { text, text + strlen(text) };
    return parse_value(&p);
}

void json_free(void) { g_ni = 0; g_si = 0; }

json_node_t *json_get(const json_node_t *obj, const char *key) {
    if (!obj || obj->type != JSON_OBJECT) return NULL;
    for (json_node_t *c = obj->v.children; c; c = c->next)
        if (c->key && strcmp(c->key, key) == 0) return c;
    return NULL;
}

const char *json_str(const json_node_t *obj, const char *key, const char *def) {
    json_node_t *n = json_get(obj, key);
    return (n && n->type == JSON_STRING) ? n->v.s : def;
}

int json_int(const json_node_t *obj, const char *key, int def) {
    json_node_t *n = json_get(obj, key);
    return (n && n->type == JSON_NUMBER) ? (int)n->v.n : def;
}

double json_num(const json_node_t *obj, const char *key, double def) {
    json_node_t *n = json_get(obj, key);
    return (n && n->type == JSON_NUMBER) ? n->v.n : def;
}

/* ---- Writer ---- */
static void jw_raw(json_writer_t *w, const char *s) {
    if (!jw_ok(w)) return;
    int slen = (int)strlen(s);
    if (w->len + slen >= w->cap) { w->len = w->cap; return; }
    memcpy(w->buf + w->len, s, slen);
    w->len += slen;
    w->buf[w->len] = '\0';
}

static void jw_sep_if_needed(json_writer_t *w) {
    if (w->need_sep) jw_raw(w, ",");
    w->need_sep = 1;
}

void jw_init(json_writer_t *w, char *buf, int cap) {
    w->buf = buf; w->len = 0; w->cap = cap; w->need_sep = 0;
    if (cap > 0) buf[0] = '\0';
}

void jw_obj_open(json_writer_t *w)  { jw_sep_if_needed(w); jw_raw(w, "{"); w->need_sep = 0; }
void jw_obj_close(json_writer_t *w) { jw_raw(w, "}"); w->need_sep = 1; }
void jw_arr_open(json_writer_t *w)  { jw_sep_if_needed(w); jw_raw(w, "["); w->need_sep = 0; }
void jw_arr_close(json_writer_t *w) { jw_raw(w, "]"); w->need_sep = 1; }

void jw_arr_obj_open(json_writer_t *w) { jw_sep_if_needed(w); jw_raw(w, "{"); w->need_sep = 0; }

void jw_key_str(json_writer_t *w, const char *k, const char *v) {
    jw_sep_if_needed(w);
    char tmp[512];
    snprintf(tmp, sizeof(tmp), "\"%s\":\"%s\"", k, v ? v : "");
    jw_raw(w, tmp);
}

void jw_key_int(json_writer_t *w, const char *k, int v) {
    jw_sep_if_needed(w);
    char tmp[128];
    snprintf(tmp, sizeof(tmp), "\"%s\":%d", k, v);
    jw_raw(w, tmp);
}

void jw_key_dbl(json_writer_t *w, const char *k, double v) {
    jw_sep_if_needed(w);
    char tmp[128];
    snprintf(tmp, sizeof(tmp), "\"%s\":%.4f", k, v);
    jw_raw(w, tmp);
}

void jw_key_obj_open(json_writer_t *w, const char *k) {
    jw_sep_if_needed(w);
    char tmp[128];
    snprintf(tmp, sizeof(tmp), "\"%s\":{", k);
    jw_raw(w, tmp);
    w->need_sep = 0;
}

void jw_key_arr_open(json_writer_t *w, const char *k) {
    jw_sep_if_needed(w);
    char tmp[128];
    snprintf(tmp, sizeof(tmp), "\"%s\":[", k);
    jw_raw(w, tmp);
    w->need_sep = 0;
}

int jw_ok(const json_writer_t *w) { return w->len < w->cap; }
