/* it.massirito.pspobd2 - PSP OBD2 diagnostics & telemetry
 * (c) massirito. Homebrew non firmato crittograficamente: gira su CFW/HEN.
 * File: json.h  |  Modulo: parser JSON minimale (config/layout su ms0)
 */
#ifndef JSON_H
#define JSON_H

typedef enum {
    JSON_NULL, JSON_BOOL, JSON_NUMBER, JSON_STRING, JSON_ARRAY, JSON_OBJECT
} json_type_t;

typedef struct json_node {
    json_type_t       type;
    const char       *key;         /* chiave se membro di un oggetto */
    union {
        int            b;          /* JSON_BOOL */
        double         n;          /* JSON_NUMBER */
        char          *s;          /* JSON_STRING */
        struct json_node *children;/* JSON_ARRAY / JSON_OBJECT (lista via ->next) */
    } v;
    struct json_node *next;        /* fratello nella lista figli del padre */
} json_node_t;

/* Parsa testo JSON. Ritorna il nodo radice o NULL in caso di errore.
 * Usa pool statico interno - non rientrante. Chiama json_free() dopo l'uso. */
json_node_t *json_parse(const char *text);
void         json_free(void);

/* Cerca figlio diretto di un OBJECT per chiave */
json_node_t *json_get(const json_node_t *obj, const char *key);

/* Helper con valori di default */
const char  *json_str(const json_node_t *obj, const char *key, const char *def);
int          json_int(const json_node_t *obj, const char *key, int def);
double       json_num(const json_node_t *obj, const char *key, double def);

/* Writer: scrive JSON in un buffer char fisso */
typedef struct {
    char *buf;
    int   len;
    int   cap;
    int   need_sep; /* 1 se il prossimo elemento ha bisogno di virgola */
} json_writer_t;

void jw_init(json_writer_t *w, char *buf, int cap);
void jw_obj_open(json_writer_t *w);
void jw_obj_close(json_writer_t *w);
void jw_arr_open(json_writer_t *w);
void jw_arr_close(json_writer_t *w);
void jw_key_str(json_writer_t *w, const char *k, const char *v);
void jw_key_int(json_writer_t *w, const char *k, int v);
void jw_key_dbl(json_writer_t *w, const char *k, double v);
/* Apre un oggetto come valore di una chiave (per oggetti annidati) */
void jw_key_obj_open(json_writer_t *w, const char *k);
/* Apre un array come valore di una chiave */
void jw_key_arr_open(json_writer_t *w, const char *k);
/* Aggiunge un oggetto anonimo nell'array corrente */
void jw_arr_obj_open(json_writer_t *w);
int  jw_ok(const json_writer_t *w);

#endif /* JSON_H */
