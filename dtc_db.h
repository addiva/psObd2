/* it.massirito.pspobd2 - PSP OBD2 diagnostics & telemetry
 * (c) massirito. Homebrew non firmato crittograficamente: gira su CFW/HEN.
 * File: dtc_db.h  |  Modulo: dizionario DTC (generici + Mazda MX-5 NC)
 */
#ifndef DTC_DB_H
#define DTC_DB_H

typedef struct {
    const char *code;        /* es. "P0301" */
    const char *description; /* descrizione breve in italiano */
    int         mx5_nc;      /* 1 = particolarmente rilevante per MX-5 NC */
} dtc_entry_t;

/* Cerca una descrizione per il codice DTC (es. "P0301").
 * Ritorna la stringa di descrizione o NULL se non trovato.
 * La ricerca e' case-insensitive e ignora separatori. */
const char *dtc_lookup(const char *code);

/* Ritorna 1 se il codice e' specifico Mazda/MX-5 NC (non standard OBD2) */
int dtc_is_manufacturer(const char *code);

/* Accesso diretto al database completo (per UI lista/ricerca) */
extern const dtc_entry_t dtc_db[];
extern const int         dtc_db_count;

#endif /* DTC_DB_H */
