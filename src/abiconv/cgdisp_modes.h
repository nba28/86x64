/* cgdisp_modes.h — the display-mode list every translated app sees
 * (cg_display_fullscreen_shim.c cgdisp_point_modes): point sizes that fit the
 * desktop, the pre-Mojave Retina contract. Shared by the CG and the classic
 * Display Manager (qd_gworld.c) list APIs so they can never disagree. */
#ifndef CGDISP_MODES_H
#define CGDISP_MODES_H
#include <stdint.h>
typedef struct { int w, h; double hz; } cgdisp_mode;
int cgdisp_point_modes(uint32_t display, cgdisp_mode *out, int max);
#endif
