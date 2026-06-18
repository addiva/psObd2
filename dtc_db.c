/* it.massirito.pspobd2 - PSP OBD2 diagnostics & telemetry
 * (c) massirito. Homebrew non firmato crittograficamente: gira su CFW/HEN.
 * File: dtc_db.c  |  Modulo: dizionario DTC (generici + Mazda MX-5 NC)
 *
 * Fonti: SAE J2012 (codici P0xxx standard), documentazione Mazda workshop
 * manual NC (2006-2015, motore MZR 2.0 LF-VE / LF-DE).
 * I codici P1xxx/P2xxx Mazda sono manufacturer-specific: alcune ECU NC
 * potrebbero usare varianti. Verificare sempre col workshop manual fisico.
 *
 * Limitazioni oneste:
 * - Database non esaustivo: copre i codici piu' comuni per uso normale.
 * - I codici P1xxx Mazda non sono standard SAE: dipendono dalla versione ECU.
 * - Il protocollo generico Mode 03 legge solo codici powertrain standard;
 *   guasti corpo vettura/ABS/airbag richiedono software diagnostico Mazda.
 */
#include "dtc_db.h"
#include <string.h>
#include <ctype.h>

/* mx5_nc=1 = particolarmente rilevante per MX-5 NC / motore MZR 2.0 */
const dtc_entry_t dtc_db[] = {

    /* ================================================================
     * SENSORI ARIA / CARBURANTE (P010x - P019x)
     * ================================================================ */
    { "P0100", "Portata aria (MAF): circuito fuori range",             1 },
    { "P0101", "Portata aria (MAF): range/prestazione",                1 },
    { "P0102", "Portata aria (MAF): segnale basso",                    1 },
    { "P0103", "Portata aria (MAF): segnale alto",                     1 },
    { "P0104", "Portata aria (MAF): circuito intermittente",           1 },
    { "P0105", "Sensore pressione assoluta collettore (MAP): guasto",  0 },
    { "P0107", "Sensore MAP: segnale basso",                           0 },
    { "P0108", "Sensore MAP: segnale alto",                            0 },
    { "P0110", "Sensore temperatura aria aspirata (IAT): circuito",    1 },
    { "P0111", "Sensore IAT: range/prestazione",                       1 },
    { "P0112", "Sensore IAT: segnale basso",                           1 },
    { "P0113", "Sensore IAT: segnale alto",                            1 },
    { "P0115", "Sensore temperatura liquido refrigerante (ECT): circ.",1 },
    { "P0116", "Sensore ECT: range/prestazione",                       1 },
    { "P0117", "Sensore ECT: segnale basso",                           1 },
    { "P0118", "Sensore ECT: segnale alto",                            1 },
    { "P0119", "Sensore ECT: segnale intermittente",                   1 },
    { "P0120", "Sensore posizione farfalla (TPS): circuito A",         1 },
    { "P0121", "Sensore TPS: range/prestazione A",                     1 },
    { "P0122", "Sensore TPS: segnale basso A",                         1 },
    { "P0123", "Sensore TPS: segnale alto A",                          1 },
    { "P0125", "Temperatura insufficiente per closed-loop (ECT)",      1 },
    { "P0128", "Termostato: temp refrigerante sotto soglia modello",   1 },

    /* ================================================================
     * SONDE LAMBDA / OSSIGENO (P013x - P016x)
     * ================================================================ */
    { "P0130", "Sonda O2 banco 1 sensore 1 (pre-catalizzatore): circ.",1 },
    { "P0131", "Sonda O2 B1S1: segnale basso (miscela magra)",         1 },
    { "P0132", "Sonda O2 B1S1: segnale alto (miscela ricca)",          1 },
    { "P0133", "Sonda O2 B1S1: risposta lenta",                        1 },
    { "P0134", "Sonda O2 B1S1: nessuna attivita'",                     1 },
    { "P0135", "Sonda O2 B1S1: riscaldatore circuito",                 1 },
    { "P0136", "Sonda O2 banco 1 sensore 2 (post-cat): circuito",      1 },
    { "P0137", "Sonda O2 B1S2: segnale basso",                         1 },
    { "P0138", "Sonda O2 B1S2: segnale alto",                          1 },
    { "P0139", "Sonda O2 B1S2: risposta lenta",                        1 },
    { "P0141", "Sonda O2 B1S2: riscaldatore circuito",                 1 },

    /* ================================================================
     * CORREZIONE CARBURANTE / FUEL TRIM (P017x)
     * ================================================================ */
    { "P0170", "Correzione carburante banco 1: guasto",                1 },
    { "P0171", "Miscela magra banco 1 (fuel trim oltre limite +)",     1 },
    { "P0172", "Miscela ricca banco 1 (fuel trim oltre limite -)",     1 },

    /* ================================================================
     * INIETTORI (P020x - P027x)
     * ================================================================ */
    { "P0200", "Circuito iniettore: guasto generico",                  1 },
    { "P0201", "Circuito iniettore cilindro 1",                        1 },
    { "P0202", "Circuito iniettore cilindro 2",                        1 },
    { "P0203", "Circuito iniettore cilindro 3",                        1 },
    { "P0204", "Circuito iniettore cilindro 4",                        1 },
    { "P0261", "Iniettore cilindro 1: segnale basso",                  1 },
    { "P0262", "Iniettore cilindro 1: segnale alto",                   1 },
    { "P0264", "Iniettore cilindro 2: segnale basso",                  1 },
    { "P0265", "Iniettore cilindro 2: segnale alto",                   1 },
    { "P0267", "Iniettore cilindro 3: segnale basso",                  1 },
    { "P0268", "Iniettore cilindro 3: segnale alto",                   1 },
    { "P0270", "Iniettore cilindro 4: segnale basso",                  1 },
    { "P0271", "Iniettore cilindro 4: segnale alto",                   1 },

    /* ================================================================
     * MANCATE ACCENSIONI / MISFIRE (P030x)
     * MX-5 NC: motore 4 cil. -> solo cil. 1-4 rilevanti
     * ================================================================ */
    { "P0300", "Mancate accensioni casuali/multiple cilindri",         1 },
    { "P0301", "Mancata accensione cilindro 1",                        1 },
    { "P0302", "Mancata accensione cilindro 2",                        1 },
    { "P0303", "Mancata accensione cilindro 3",                        1 },
    { "P0304", "Mancata accensione cilindro 4",                        1 },

    /* ================================================================
     * ACCENSIONE / TIMING (P032x - P034x)
     * ================================================================ */
    { "P0320", "Circuito giri motore (CKP): guasto",                   1 },
    { "P0325", "Sensore detonazione banco 1: circuito",                1 },
    { "P0326", "Sensore detonazione banco 1: range/prestazione",       1 },
    { "P0327", "Sensore detonazione banco 1: segnale basso",           1 },
    { "P0328", "Sensore detonazione banco 1: segnale alto",            1 },
    { "P0335", "Sensore posizione albero motore (CKP): circuito A",    1 },
    { "P0336", "Sensore CKP: range/prestazione A",                     1 },
    { "P0339", "Sensore CKP: segnale intermittente A",                 1 },
    { "P0340", "Sensore posizione albero a camme (CMP): circuito A",   1 },
    { "P0341", "Sensore CMP: range/prestazione A",                     1 },
    { "P0342", "Sensore CMP: segnale basso A",                         1 },
    { "P0343", "Sensore CMP: segnale alto A",                          1 },

    /* ================================================================
     * VARIABLE VALVE TIMING - VVT (P001x - P002x)
     * MX-5 NC ha VVT sull'albero a camme di aspirazione
     * ================================================================ */
    { "P0010", "Attuatore VVT aspirazione banco 1: circuito",          1 },
    { "P0011", "VVT aspirazione banco 1: fasatura in anticipo eccessivo",1 },
    { "P0012", "VVT aspirazione banco 1: fasatura in ritardo eccessivo",1 },
    { "P0013", "Attuatore VVT scarico banco 1: circuito",              0 },
    { "P0014", "VVT scarico banco 1: fasatura in anticipo eccessivo",  0 },
    { "P0015", "VVT scarico banco 1: fasatura in ritardo eccessivo",   0 },

    /* ================================================================
     * CATALIZZATORE (P042x)
     * ================================================================ */
    { "P0420", "Efficienza catalizzatore banco 1 sotto soglia",        1 },
    { "P0421", "Efficienza catalizzatore banco 1: caldo sotto soglia", 1 },

    /* ================================================================
     * SISTEMA EVAP / VAPORI CARBURANTE (P044x - P046x)
     * ================================================================ */
    { "P0440", "Sistema EVAP: guasto generico",                        1 },
    { "P0441", "Sistema EVAP: controllo portata non corretto",         1 },
    { "P0442", "Sistema EVAP: perdita piccola rilevata",               1 },
    { "P0443", "Valvola spurgo canister EVAP: circuito",               1 },
    { "P0446", "Valvola ventilazione canister EVAP: circuito",         1 },
    { "P0447", "Valvola ventilazione canister EVAP: aperta",           1 },
    { "P0448", "Valvola ventilazione canister EVAP: chiusa",           1 },
    { "P0449", "Valvola ventilazione EVAP: circuito",                  1 },
    { "P0455", "Sistema EVAP: perdita grande rilevata",                1 },
    { "P0456", "Sistema EVAP: perdita molto piccola rilevata",         1 },

    /* ================================================================
     * SENSORE VELOCITA' / CONTROLLO MINIMO (P050x)
     * ================================================================ */
    { "P0500", "Sensore velocita' veicolo (VSS): guasto",              1 },
    { "P0501", "Sensore VSS: range/prestazione",                       1 },
    { "P0505", "Sistema controllo regime minimo (IAC): guasto",        1 },
    { "P0506", "Regime minimo inferiore al target",                    1 },
    { "P0507", "Regime minimo superiore al target",                    1 },

    /* ================================================================
     * COMUNICAZIONE / ECU (P060x - P069x)
     * ================================================================ */
    { "P0600", "Seriale link di comunicazione: guasto",                0 },
    { "P0601", "ECU memoria interna KAM: guasto",                     1 },
    { "P0602", "ECU: modulo controllo non programmato",               1 },
    { "P0605", "ECU ROM: guasto interno",                              1 },
    { "P0606", "ECU processore: guasto",                               1 },

    /* ================================================================
     * TRASMISSIONE (P070x - P079x)
     * MX-5 NC: cambio manuale (P07xx meno rilevanti) o automatico
     * ================================================================ */
    { "P0700", "Sistema controllo trasmissione: MIL richiesta",        0 },
    { "P0715", "Sensore giri ingresso trasmissione: circuito",         0 },
    { "P0720", "Sensore giri uscita trasmissione: circuito",           0 },
    { "P0730", "Rapporto ingranaggio non corretto",                    0 },
    { "P0740", "Circuito solenoide blocco convertitore: guasto",       0 },
    { "P0741", "Blocco convertitore: prestazione/bloccato aperto",     0 },
    { "P0743", "Circuito solenoide blocco convertitore: elettrico",    0 },
    { "P0748", "Solenoide pressione trasmissione A: elettrico",        0 },
    { "P0751", "Solenoide cambio A: stuck off",                        0 },
    { "P0756", "Solenoide cambio B: stuck off",                        0 },
    { "P0760", "Solenoide cambio C: circuito",                         0 },
    { "P0780", "Problema cambio marcia",                               0 },

    /* ================================================================
     * CODICI MAZDA SPECIFICI P1xxx
     * Motore MZR 2.0 LF-VE/LF-DE (MX-5 NC 2006-2015)
     * AVVISO: manufacturer-specific, non standard SAE.
     * Verificare con Mazda workshop manual prima di intervenire.
     * ================================================================ */
    { "P1100", "MAF sensore: intermittente",                           1 },
    { "P1101", "MAF sensore: fuori range self-test",                   1 },
    { "P1120", "TPS: fuori range basso",                               1 },
    { "P1121", "TPS: variazione inattesa rispetto MAP",                1 },
    { "P1170", "Sonda O2 B1S1: segnale bloccato (non oscilla)",        1 },
    { "P1171", "Sonda O2 B1S1: segnale magro durante power enrichment",1 },
    { "P1195", "Tensione riferimento sonda O2: fuori range",           1 },
    { "P1235", "Pompa carburante: circuito fuori range self-test",     1 },
    { "P1236", "Pompa carburante: circuito self-test non completato",  1 },
    { "P1260", "Furto rilevato - motore immobilizzato (immobilizer)",  1 },
    { "P1285", "Temperatura testa motore: troppo elevata",             1 },
    { "P1288", "Sensore temperatura testa: fuori range self-test",     1 },
    { "P1289", "Sensore temperatura testa: segnale basso",             1 },
    { "P1290", "Sensore temperatura testa: segnale alto",              1 },
    { "P1309", "Rilevamento mancata accensione: sistema spento",       1 },
    { "P1345", "Timing troppo ritardato (over-retarded)",              1 },
    { "P1381", "Variazione timing: troppo avanzato oltre limite",      1 },
    { "P1383", "Variazione timing: troppo ritardato oltre limite",     1 },
    { "P1386", "Controllo detonazione: cylinder #1 guasto knock",      1 },
    { "P1450", "Sensore pressione barometrica (BARO): guasto",         1 },
    { "P1460", "Sensore posizione farfalla: fuori range basso WOT",    1 },
    { "P1461", "Sensore posizione farfalla: fuori range alto idle",    1 },
    { "P1462", "Sistema AC: sensore pressione refrigerante basso",     1 },
    { "P1463", "Sistema AC: sensore pressione refrigerante alto",      1 },
    { "P1464", "Sistema AC: limite corrente in test",                  1 },
    { "P1473", "Valvola aggiuntiva aria (SAI): circuito off",          1 },
    { "P1487", "Solenoide spurgo EGR boost check",                     0 },
    { "P1496", "Motore passo-passo EGR: circuito",                     0 },
    { "P1500", "VSS sensore velocita': intermittente",                 1 },
    { "P1501", "VSS fuori range self-test",                            1 },
    { "P1502", "Sensore velocita' veicolo: range alto self-test",      1 },
    { "P1507", "IAC sistema: giri minimo inferiore al previsto",       1 },
    { "P1508", "IAC sistema: giri minimo superiore al previsto",       1 },
    { "P1549", "Solenoide controllo boost: circuito off",              0 },
    { "P1600", "PCM: perdita alimentazione KAM",                       1 },
    { "P1601", "PCM: comunicazione seriale con modulo esterno",        1 },
    { "P1605", "PCM: keep alive memory (KAM) test",                    1 },
    { "P1609", "PCM: errore comunicazione seriale interna",            1 },
    { "P1633", "Tensione alimentazione KAM troppo bassa",              1 },
    { "P1635", "Tensione riferimento sensori 5V (VP): fuori range",    1 },
    { "P1639", "Tensione riferimento sensori 5V (VP2): fuori range",   1 },
    { "P1700", "Segnale richiesta shift trasmissione: intermittente",  0 },
    { "P1701", "Segnale richiesta shift: fuori range",                 0 },
    { "P1703", "Freno: segnale out-of-self-test range",                0 },
    { "P1705", "Sensore posizione cambio: fuori range (P/N)",          0 },
    { "P1709", "Segnale Park/Neutral: fuori range self-test",          0 },
    { "P1711", "Sensore temperatura olio trasmissione (TFT): range",   0 },
    { "P1714", "Solenoide shift A: guasto meccanico stuck off",        0 },
    { "P1715", "Solenoide shift A: guasto meccanico stuck on",         0 },
    { "P1716", "Solenoide shift B: guasto meccanico stuck off",        0 },
    { "P1717", "Solenoide shift B: guasto meccanico stuck on",         0 },
    { "P1718", "Solenoide shift C: guasto meccanico stuck off",        0 },
    { "P1743", "Solenoide blocco convertitore: guasto meccanico",      0 },
    { "P1744", "Blocco convertitore: performance",                     0 },
    { "P1746", "Solenoide pressione EPC elettronico: corto a terra",   0 },
    { "P1747", "Solenoide pressione EPC: corto a batteria",            0 },
    { "P1748", "Solenoide pressione EPC: fuori range self-test",       0 },
    { "P1751", "Solenoide shift A: guasto prestazione",                0 },
    { "P1756", "Solenoide shift B: guasto prestazione",                0 },
    { "P1780", "Interruttore Park/Neutral: fuori self-test range",     0 },
    { "P1781", "Interruttore 4x4 Low: fuori self-test range",          0 },
    { "P1783", "Temperatura trasmissione eccessiva",                   0 },

    /* ================================================================
     * CODICI P2xxx RILEVANTI MX-5 NC
     * Include IMRC (Intake Manifold Runner Control) - presente sul MZR 2.0
     * ================================================================ */
    { "P2004", "IMRC (controllo runner collettore aspirazione) stuck aperto",  1 },
    { "P2005", "IMRC banco 2: stuck aperto",                           0 },
    { "P2006", "IMRC (controllo runner collettore aspirazione) stuck chiuso",  1 },
    { "P2007", "IMRC banco 2: stuck chiuso",                           0 },
    { "P2008", "Circuito solenoide IMRC banco 1: aperto",              1 },
    { "P2009", "Circuito solenoide IMRC banco 1: basso/terra",         1 },
    { "P2010", "Circuito solenoide IMRC banco 1: alto",                1 },
    { "P2096", "Correzione carburante post-catalizzatore: troppo magra",1 },
    { "P2097", "Correzione carburante post-catalizzatore: troppo ricca",1 },
    { "P2101", "Attuatore farfalla elettronica (ETC): range circuito", 1 },
    { "P2102", "Attuatore ETC: segnale basso",                         1 },
    { "P2103", "Attuatore ETC: segnale alto",                          1 },
    { "P2106", "ETC: funzionamento in modalita' limitata",             1 },
    { "P2107", "Modulo controllo farfalla: processore interno",        1 },
    { "P2108", "Modulo controllo farfalla: guasto",                    1 },
    { "P2111", "Sistema ETC: stuck aperto",                            1 },
    { "P2112", "Sistema ETC: stuck chiuso",                            1 },
    { "P2119", "Corpo farfalla: posizione non corretta",               1 },
    { "P2122", "Pedale acceleratore sensore D: segnale basso",        1 },
    { "P2123", "Pedale acceleratore sensore D: segnale alto",         1 },
    { "P2127", "Pedale acceleratore sensore E: segnale basso",        1 },
    { "P2128", "Pedale acceleratore sensore E: segnale alto",         1 },
    { "P2135", "Segnali TPS A/B non correlati",                        1 },
    { "P2138", "Segnali pedale acceleratore D/E non correlati",       1 },
    { "P2195", "Sonda O2 B1S1: segnale bloccato magro",               1 },
    { "P2196", "Sonda O2 B1S1: segnale bloccato ricco",               1 },
    { "P2270", "Sonda O2 B1S2: segnale bloccato magro",               1 },
    { "P2271", "Sonda O2 B1S2: segnale bloccato ricco",               1 },
    { "P2300", "Circuito primario bobina cil. 1: segnale basso",       1 },
    { "P2301", "Circuito primario bobina cil. 1: segnale alto",        1 },
    { "P2303", "Circuito primario bobina cil. 2: segnale basso",       1 },
    { "P2304", "Circuito primario bobina cil. 2: segnale alto",        1 },
    { "P2306", "Circuito primario bobina cil. 3: segnale basso",       1 },
    { "P2307", "Circuito primario bobina cil. 3: segnale alto",        1 },
    { "P2309", "Circuito primario bobina cil. 4: segnale basso",       1 },
    { "P2310", "Circuito primario bobina cil. 4: segnale alto",        1 },
};

const int dtc_db_count = (int)(sizeof(dtc_db) / sizeof(dtc_db[0]));

/* ---- Lookup ---- */

const char *dtc_lookup(const char *code) {
    if (!code) return NULL;
    /* Normalizza: maiuscolo, salta spazi */
    char norm[8] = {0};
    int ni = 0;
    for (const char *p = code; *p && ni < 7; p++) {
        if (isalnum((unsigned char)*p))
            norm[ni++] = (char)toupper((unsigned char)*p);
    }
    norm[ni] = '\0';
    for (int i = 0; i < dtc_db_count; i++) {
        if (strcmp(dtc_db[i].code, norm) == 0)
            return dtc_db[i].description;
    }
    return NULL;
}

int dtc_is_manufacturer(const char *code) {
    if (!code || code[0] == '\0') return 0;
    /* Codici P1xxx, P2xxx (alcuni), C/B/U1xxx sono manufacturer-specific */
    char first = (char)toupper((unsigned char)code[0]);
    if ((first == 'P' || first == 'C' || first == 'B' || first == 'U') &&
        code[1] == '1')
        return 1;
    return 0;
}
