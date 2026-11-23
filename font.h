/* it.massirito.pspobd2 - PSP OBD2 diagnostics & telemetry
 * (c) massirito. Homebrew non firmato crittograficamente: gira su CFW/HEN.
 * File: font.h  |  Modulo: rasterizzatore testo bitmap (portabile, testabile su PC)
 */
#ifndef FONT_H
#define FONT_H

#include <stdint.h>

/* Glifi ASCII 32..126, 8 righe ciascuno, MSB = colonna sinistra.
 * I glifi occupano 5x7 pixel: avanzamento orizzontale 6 px per scala. */
extern const unsigned char font8x8[95][8];

#define FONT_ADV   6   /* avanzamento per carattere (scala 1) */
#define FONT_H_PX  8   /* altezza riga (scala 1) */

/* Larghezza in pixel di una stringa alla scala data */
int  font_text_width(const char *s, int scale);

/* Disegna una stringa in un framebuffer 32 bit (pixel 0xAABBGGRR).
 * fb_w/fb_h = area visibile per il clipping, stride = pixel per riga.
 * Scrive solo i pixel accesi (sfondo trasparente). */
void font_draw(uint32_t *fb, int stride, int fb_w, int fb_h,
               int x, int y, uint32_t color, int scale, const char *s);

#endif /* FONT_H */
