/* it.massirito.pspobd2 - PSP OBD2 diagnostics & telemetry
 * (c) massirito. Homebrew non firmato crittograficamente: gira su CFW/HEN.
 * File: gfx.h  |  Modulo: grafica 2D (GU su PSP, rasterizzatore software su PC)
 *
 * Modello di frame:
 *   gfx_begin(clear)  -> rettangoli/archi/immagini vanno in coda al GE
 *   gfx_text(...)     -> il testo viene accodato e disegnato DOPO la geometria
 *   gfx_end()         -> attende il GE, scrive il testo in VRAM, vsync, swap
 * Colori: 0xAABBGGRR (formato nativo PSP).
 */
#ifndef GFX_H_INCLUDED
#define GFX_H_INCLUDED

#include <stdint.h>

#define GFX_W      480
#define GFX_H      272
#define GFX_STRIDE 512

int  gfx_init(void);
void gfx_shutdown(void);

void gfx_begin(uint32_t clear_color);
void gfx_end(void);

void gfx_rect(int x, int y, int w, int h, uint32_t color);
void gfx_frame(int x, int y, int w, int h, int t, uint32_t color); /* contorno */
/* Arco ad anello: angoli in gradi, 0 = destra, 90 = basso (coordinate schermo) */
void gfx_arc(int cx, int cy, int r, int thickness,
             float from_deg, float to_deg, uint32_t color);
/* Immagine RGBA (stride in pixel) scalata nel rettangolo di destinazione */
void gfx_image(const uint32_t *rgba, int w, int h, int stride,
               int dx, int dy, int dw, int dh);

/* Testo (accodato, sempre sopra la geometria del frame) */
void gfx_text(int x, int y, uint32_t color, int scale, const char *s);
int  gfx_text_w(const char *s, int scale);

#ifndef PSP_BUILD
/* Solo PC: accesso al canvas software (GFX_STRIDE x GFX_H) */
const uint32_t *gfx_soft_pixels(void);
#endif

#endif /* GFX_H_INCLUDED */
