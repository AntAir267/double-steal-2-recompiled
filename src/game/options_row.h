/*
 * options_row.h - PC SETTING, a row of the game's own OPTIONS screen that
 * opens the settings overlay (options_row.c).
 */
#ifndef DS2_OPTIONS_ROW_H
#define DS2_OPTIONS_ROW_H

#include <stdint.h>

/* The row's item number in the screen's A handler (the screen's own are
 * 0 to 9). */
#define DS2_OPTIONS_PC_ITEM 10

/* The screen resource object (thiscall `this` of sub_000EB7A0) about to
 * parse its layout: when it is Options.xpr, a layout with the row added is
 * built and the resource made to parse that one instead, and the row's
 * label and hint are drawn into the screen's textures. 1 when it did. */
int ds2_options_row_patch(uint32_t res);

/* A DXT1 (dxt 1) or DXT5 (dxt 5) texture, w x h, blocks in rows, to RGBA8
 * (r, g, b, a bytes, rows top down; free() it). NULL when out of memory. */
uint8_t *ds2_dxt_to_rgba(const uint8_t *src, int w, int h, int dxt);

#endif
