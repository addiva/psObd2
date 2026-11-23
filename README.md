# psObd2 — PSP OBD2 (it.massirito.pspobd2)

Homebrew per PSP moddata (CFW o HEN) che legge i dati dell'auto da un adattatore
**ELM327 WiFi**: giri, velocità, consumi, temperature, codici guasto (DTC) con
dizionario MX-5 NC, cronometro 0-100, registro rifornimenti e camera Go!Cam.

## Installazione

1. Copia la cartella `release/PSP` nella **radice** della Memory Stick
   (risultato: `PSP/GAME/it.massirito.pspobd2/EBOOT.PBP`).
2. PSP: *Impostazioni → Impostazioni di rete → Modalità infrastruttura*:
   crea un profilo verso la rete WiFi dell'ELM327 (es. `WiFi_OBDII`).
   Ricorda il **numero** del profilo (1 = il primo).
3. Collega l'ELM327 alla presa OBD, quadro acceso, switch WiFi della PSP acceso.
4. Avvia da *Gioco → Memory Stick*. Se serve, vai alla pagina **Setup**
   (L/R fino a 8/8), premi **X** e imposta auto, numero profilo e IP:porta
   dell'ELM (di solito `192.168.0.10 : 35000`).

Lo stato della connessione è sempre in alto a destra (verde = OK).

## Comandi

| Tasto | Azione |
|---|---|
| L / R | Pagina precedente / successiva |
| Guida: □ | Azzera trip |
| Costi: X | Registra un rifornimento (km, litri, €/L) |
| Diagnostica: X / O | Leggi DTC / cancella DTC (premere O due volte) |
| 0-100: X | Arma il cronometro: parte da solo quando l'auto si muove |
| Camera: X | Foto in `snapshots/` |
| Setup: X / □ | Modifica impostazioni / riconnetti |
| △ | Modifica layout: frecce scelgono, X mostra/nasconde, O sposta, □ ripristina, △ salva |
| START+SELECT o HOME | Esci (salva tutto) |

## File sulla Memory Stick (nella cartella dell'EBOOT)

- `config.json` — creato al primo avvio, modificabile anche dalla pagina Setup:
  ```json
  {"elm":{"ip":"192.168.0.10","port":35000,"apctl":1},"car_id":"mx5_nc",
   "last_eur_l":1.80,"km_since_fill":0,"accel_best":[0,0,0,0,0,0,0]}
  ```
  `car_id`: `mx5_nc`, `c2_hdi`, `bmw_f30`. Accettato anche il formato piatto
  `elm_ip`, `elm_port`, `elm_apctl`, `eur_per_l`.
- `layout.json` — posizione/visibilità dei widget (cancellalo per il default).
- `fuel_<auto>.csv` — rifornimenti: `date,km,liters,eur_per_l,l_per_100,eur_per_100`.

## Compilazione

```sh
make              # EBOOT.PBP (richiede pspdev nel PATH)
make check-imports  # verifica che il PRX non importi librerie kernel
make test         # test logici su PC (gcc)
make preview      # strumento di anteprima/test su PC:
./ui_preview demo /tmp/shots                 # PNG di tutte le pagine (tools/ppm2png.py)
python3 tools/fake_elm.py 35123 &            # ELM327 simulato (MX-5, CAN)
./ui_preview e2e 35123 /tmp/shots            # test end-to-end del thread OBD
```
