/*
 * options_row.c - PC SETTING, a row of the game's own OPTIONS screen that
 * opens the settings overlay, as Burnout 3's TAKEDAWN SETTINGS does
 * (settings.c says whether there is one).
 *
 * The screen is data. data\Screen\Options.xpr (in the player's
 * pkfiles0.<lang>.pk) holds a layout and two DXT textures: Options.tga
 * (1024x512 DXT5: the labels, values, arrows and the panel) and Help.tga
 * (512x512 DXT1: the hint lines, English or Japanese). The layout lists the
 * pictures, then one page of elements and animations; the screen's code
 * finds elements by name and switches on an element's item number, and the
 * menu code moves the cursor by the up and down links the elements carry.
 * So a row is data plus one case: when the resource is about to parse its
 * layout (sub_000EB7A0, recomp_hooks.c), a copy is made with the row added
 * and parsed instead, and A on the row opens the overlay (sub_0010A900).
 *
 * The layout block, offsets from its start, names from its string pool:
 *   +04 +08  textures: count, offset (8 bytes each: name, texture)
 *   +0C +10  pictures: count, offset (24: texture index, u0 | v0 << 16,
 *            u1 | v1 << 16 in 1/4096 of the texture, width | height << 16
 *            on screen, 0, 1)
 *   +14 +18  picture sets: count, offset (offsets of a count and indices)
 *   +1C +20  texts: count, offset (8: the first text, a style)
 *   +24 +28  pages: count, offset (offsets of the page records)
 *   +2C      the string pool
 * A page: name, element count and offset (from the page), two zero words,
 * animation count and offset (from the page), 0xFFFF0000. An element (36
 * bytes): name, kind | picture << 16, alignment | x << 16, y | 100 << 16,
 * colour, item | up << 16, down, parent << 16, select animation | deselect
 * animation << 16 (up and down: element index deltas). An animation: name,
 * track count, track list offset, the list's entries the tracks' offsets,
 * all from the animation's own record. A track: type, size, then for 3
 * (move) and 6 (colour) element, start, curve, end frame, from, to (a
 * position y << 16 | x), for 8 and 9 (start, stop) an animation's index.
 *
 * The row goes under CONTROLLER SETTING, the rows below it one down; ten
 * rows take 31 lines each where nine took 33. Its label and hint are cut
 * from the screen's own lettering, at run time from the player's copy
 * (nothing of it is in this repository): PC from the P of GUIDE MAP and
 * the C of MUSIC VOLUME, then SETTING from CONTROLLER SETTING; the hint
 * "PC version settings" from words of the other hints, or in Japanese
 * 設定画面へ from the end of the controller setting's. They are drawn into
 * the textures' empty parts: the label under the labels, the hint under
 * the hints, and the new layout after it, in rows no picture uses (the
 * resource's own memory, freed with it).
 */
#include "options_row.h"

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define LOGP "[DS2] "

extern ptrdiff_t g_xbox_mem_offset;
#define GP(a) ((uint8_t *)((uintptr_t)(uint32_t)(a) + g_xbox_mem_offset))

static uint32_t rd32(uint32_t a) { uint32_t v; memcpy(&v, GP(a), 4); return v; }
static void wr32(uint32_t a, uint32_t v) { memcpy(GP(a), &v, 4); }

/* What this layout is (anything else is left alone). */
#define N_SPR     32
#define N_ELEM    46
#define N_ANIM    28
#define E_CTRL    38                  /* ControllerSetting; 39 its hint */
#define E_RESET   40
#define E_EXIT    42
#define A_CTRL    19                  /* Sel_, Flash_, UnSel_ControllerSetting */
#define TEX0_FMT  0x09A10F29u         /* 1024x512 DXT5 */
#define TEX1_FMT  0x09910C29u         /* 512x512 DXT1 */

/* What is added: two pictures, two elements, three animations. */
#define SPR_LABEL N_SPR               /* 32: the label, Options.tga (0, 480)-(300, 510) */
#define SPR_HINT  (N_SPR + 1)         /* 33: the hint, Help.tga (0, 384)-(512, 416) */
#define E_PC      N_ELEM              /* 46; 47 its hint */
#define A_PC      N_ANIM              /* 28 Sel_, 29 Flash_, 30 UnSel_PcSetting */

/* The rows: nine at 77 + 33 n (a value 3 lower), ten at 77 + 31 n. */
#define ROW_Y0    77
#define ROW_OLD   33
#define ROW_NEW   31
#define PC_ROW    7

static uint32_t remap_y(uint32_t y)
{
    int i, d;

    if (y < 60 || y > 365)            /* the title, the hint line, the prompts */
        return y;
    i = ((int)y - ROW_Y0 + ROW_OLD / 2) / ROW_OLD;
    if (i < 0 || i > 8)
        return y;
    d = (int)y - (ROW_Y0 + ROW_OLD * i);
    if (i >= PC_ROW)
        i++;
    return (uint32_t)(ROW_Y0 + ROW_NEW * i + d);
}

/* ── DXT ───────────────────────────────────────────────────────────── */

typedef struct { uint8_t r, g, b, a; } px;

static void c565(uint16_t c, px *o)
{
    int r = c >> 11 & 31, g = c >> 5 & 63, b = c & 31;
    o->r = (uint8_t)(r << 3 | r >> 2);
    o->g = (uint8_t)(g << 2 | g >> 4);
    o->b = (uint8_t)(b << 3 | b >> 2);
    o->a = 255;
}

static uint16_t to565(px p)
{
    return (uint16_t)((p.r * 31 + 127) / 255 << 11 | (p.g * 63 + 127) / 255 << 5 | (p.b * 31 + 127) / 255);
}

static px mix(px a, px b, int wa, int wb)
{
    px o;
    int w = wa + wb;
    o.r = (uint8_t)((a.r * wa + b.r * wb) / w);
    o.g = (uint8_t)((a.g * wa + b.g * wb) / w);
    o.b = (uint8_t)((a.b * wa + b.b * wb) / w);
    o.a = 255;
    return o;
}

/* The four colours of a block; `four` 0 is DXT1's three and transparent. */
static void palette(uint16_t c0, uint16_t c1, int four, px pal[4])
{
    c565(c0, &pal[0]);
    c565(c1, &pal[1]);
    if (four) {
        pal[2] = mix(pal[0], pal[1], 2, 1);
        pal[3] = mix(pal[0], pal[1], 1, 2);
    } else {
        pal[2] = mix(pal[0], pal[1], 1, 1);
        pal[3].r = pal[3].g = pal[3].b = pal[3].a = 0;
    }
}

/* A whole texture, w x h, to RGBA. dxt 1 or 5. */
static px *dxt_decode(const uint8_t *src, int w, int h, int dxt)
{
    px *img = calloc((size_t)w * h, sizeof *img);
    int bx, by, i, bs = dxt == 1 ? 8 : 16;

    if (!img)
        return NULL;
    for (by = 0; by < h / 4; by++)
        for (bx = 0; bx < w / 4; bx++) {
            const uint8_t *b = src + ((size_t)by * (w / 4) + bx) * bs;
            const uint8_t *c = dxt == 1 ? b : b + 8;
            uint16_t c0 = (uint16_t)(c[0] | c[1] << 8), c1 = (uint16_t)(c[2] | c[3] << 8);
            uint32_t idx = (uint32_t)c[4] | (uint32_t)c[5] << 8 | (uint32_t)c[6] << 16 | (uint32_t)c[7] << 24;
            uint8_t al[8];
            uint64_t ai = 0;
            px pal[4];

            palette(c0, c1, dxt != 1 || c0 > c1, pal);
            if (dxt == 5) {
                al[0] = b[0];
                al[1] = b[1];
                if (al[0] > al[1])
                    for (i = 1; i < 7; i++)
                        al[i + 1] = (uint8_t)(((7 - i) * al[0] + i * al[1]) / 7);
                else {
                    for (i = 1; i < 5; i++)
                        al[i + 1] = (uint8_t)(((5 - i) * al[0] + i * al[1]) / 5);
                    al[6] = 0;
                    al[7] = 255;
                }
                for (i = 0; i < 6; i++)
                    ai |= (uint64_t)b[2 + i] << (8 * i);
            }
            for (i = 0; i < 16; i++) {
                px p = pal[idx >> (2 * i) & 3];
                if (dxt == 5)
                    p.a = al[ai >> (3 * i) & 7];
                img[(size_t)(by * 4 + i / 4) * w + bx * 4 + i % 4] = p;
            }
        }
    return img;
}

uint8_t *ds2_dxt_to_rgba(const uint8_t *src, int w, int h, int dxt)
{
    return (uint8_t *)dxt_decode(src, w, h, dxt);   /* px is r, g, b, a bytes */
}

static int dist2(px a, px b)
{
    int dr = a.r - b.r, dg = a.g - b.g, db = a.b - b.b;
    return dr * dr + dg * dg + db * db;
}

/* The colour half of a block: the two colours farthest apart among the
 * pixels that show (alpha > 0; for DXT1 >= 128), the rest the nearest of
 * the palette. DXT1 with a transparent pixel takes the three-colour mode. */
static void enc_color(const px p[16], int dxt1, uint8_t out[8])
{
    int i, j, best = -1, e0 = 0, e1 = 0, transp = 0, any = 0;
    uint16_t c0, c1;
    uint32_t idx = 0;
    px pal[4];

    for (i = 0; i < 16; i++) {
        int shows = dxt1 ? p[i].a >= 128 : p[i].a > 0;
        if (!shows) {
            transp = 1;
            continue;
        }
        any = 1;
        for (j = i; j < 16; j++) {
            int js = dxt1 ? p[j].a >= 128 : p[j].a > 0;
            int d = js ? dist2(p[i], p[j]) : -1;
            if (d > best) {
                best = d;
                e0 = i;
                e1 = j;
            }
        }
    }
    c0 = any ? to565(p[e0]) : 0;
    c1 = any ? to565(p[e1]) : 0;
    if (dxt1 && (transp || c0 == c1)) {
        uint16_t t;
        if (c0 > c1) { t = c0; c0 = c1; c1 = t; }      /* c0 <= c1: three colours */
        palette(c0, c1, 0, pal);
        for (i = 0; i < 16; i++) {
            int k, bk = 0, bd = 1 << 30;
            if (p[i].a < 128) {
                idx |= 3u << (2 * i);
                continue;
            }
            for (k = 0; k < 3; k++)
                if (dist2(p[i], pal[k]) < bd) { bd = dist2(p[i], pal[k]); bk = k; }
            idx |= (uint32_t)bk << (2 * i);
        }
    } else {
        uint16_t t;
        if (dxt1 && c0 < c1) { t = c0; c0 = c1; c1 = t; }      /* c0 > c1: four */
        palette(c0, c1, 1, pal);
        for (i = 0; i < 16; i++) {
            int k, bk = 0, bd = 1 << 30;
            for (k = 0; k < 4; k++)
                if (dist2(p[i], pal[k]) < bd) { bd = dist2(p[i], pal[k]); bk = k; }
            idx |= (uint32_t)bk << (2 * i);
        }
    }
    out[0] = (uint8_t)c0; out[1] = (uint8_t)(c0 >> 8);
    out[2] = (uint8_t)c1; out[3] = (uint8_t)(c1 >> 8);
    out[4] = (uint8_t)idx; out[5] = (uint8_t)(idx >> 8);
    out[6] = (uint8_t)(idx >> 16); out[7] = (uint8_t)(idx >> 24);
}

/* DXT5's alpha half: eight steps between the extremes, or six between
 * those of the rest with 0 and 255 exact, whichever is nearer. */
static int enc_alpha_mode(const px p[16], uint8_t a0, uint8_t a1, uint8_t out[8])
{
    uint8_t al[8];
    uint64_t ai = 0;
    int i, k, err = 0;

    al[0] = a0;
    al[1] = a1;
    if (a0 > a1)
        for (i = 1; i < 7; i++)
            al[i + 1] = (uint8_t)(((7 - i) * a0 + i * a1) / 7);
    else {
        for (i = 1; i < 5; i++)
            al[i + 1] = (uint8_t)(((5 - i) * a0 + i * a1) / 5);
        al[6] = 0;
        al[7] = 255;
    }
    for (i = 0; i < 16; i++) {
        int bk = 0, bd = 1 << 30;
        for (k = 0; k < 8; k++) {
            int d = (p[i].a - al[k]) * (p[i].a - al[k]);
            if (d < bd) { bd = d; bk = k; }
        }
        err += bd;
        ai |= (uint64_t)bk << (3 * i);
    }
    out[0] = a0;
    out[1] = a1;
    for (i = 0; i < 6; i++)
        out[2 + i] = (uint8_t)(ai >> (8 * i));
    return err;
}

static void enc_alpha(const px p[16], uint8_t out[8])
{
    uint8_t lo = 255, hi = 0, mlo = 255, mhi = 0, b8[8];
    int i, e8, e6;

    for (i = 0; i < 16; i++) {
        uint8_t a = p[i].a;
        if (a < lo) lo = a;
        if (a > hi) hi = a;
        if (a != 0 && a != 255) {
            if (a < mlo) mlo = a;
            if (a > mhi) mhi = a;
        }
    }
    if (mlo > mhi)                    /* only 0 and 255 */
        mlo = mhi = 0;
    e8 = hi > lo ? enc_alpha_mode(p, hi, lo, b8) : 1 << 30;
    e6 = enc_alpha_mode(p, mlo, mhi, out);
    if (e8 < e6)
        memcpy(out, b8, 8);
}

/* An RGBA picture w x h (multiples of 4) into a texture's blocks at block
 * (bx0, by0); `tw` the texture's width. */
static void dxt_encode(const px *img, int w, int h, uint8_t *tex, int tw, int bx0, int by0, int dxt)
{
    int bx, by, i, bs = dxt == 1 ? 8 : 16;

    for (by = 0; by < h / 4; by++)
        for (bx = 0; bx < w / 4; bx++) {
            uint8_t *b = tex + ((size_t)(by0 + by) * (tw / 4) + bx0 + bx) * bs;
            px blk[16];
            for (i = 0; i < 16; i++)
                blk[i] = img[(size_t)(by * 4 + i / 4) * w + bx * 4 + i % 4];
            if (dxt == 5) {
                enc_alpha(blk, b);
                enc_color(blk, 0, b + 8);
            } else {
                enc_color(blk, 1, b);
            }
        }
}

/* ── the row's pictures ────────────────────────────────────────────── */

/* A piece of lettering: `h` rows from (x, y) of the source picture, each
 * row's first column from `left` (or x0 for all), to x1, put down shifted
 * by dx, dy. Over what is there already. */
typedef struct {
    int y, h, x0, x1, dx, dy;
    const unsigned short *left;       /* per row, or NULL */
} piece;

static void put(px *dst, int dw, int dh, const px *src, int sw, const piece *p)
{
    int k, x;

    for (k = 0; k < p->h; k++) {
        int ty = k + p->dy, l = p->left ? p->left[k] : p->x0;
        if (ty < 0 || ty >= dh)
            continue;
        for (x = l; x <= p->x1; x++) {
            px s = src[(size_t)(p->y + k) * sw + x], *d;
            int tx = x + p->dx, a, under, oa;
            if (tx < 0 || tx >= dw || !s.a)
                continue;
            /* Straight alpha, source over: what shows of the pixel under. */
            d = &dst[(size_t)ty * dw + tx];
            a = s.a;
            under = d->a * (255 - a) / 255;
            oa = a + under;
            d->r = (uint8_t)((s.r * a + d->r * under + oa / 2) / oa);
            d->g = (uint8_t)((s.g * a + d->g * under + oa / 2) / oa);
            d->b = (uint8_t)((s.b * a + d->b * under + oa / 2) / oa);
            d->a = (uint8_t)oa;
        }
    }
}

/* The label's two italic cuts, per row of a 30-row cell: the first column
 * of the P of GUIDE MAP (after the A's outline) and of the C of MUSIC
 * VOLUME (after the I's), measured on the letters' slant. */
static const unsigned short k_p_left[30] = {
    280, 280, 280, 280, 280, 280, 280, 280, 280, 280, 280, 280, 280,
    279, 279, 279, 279, 279, 278, 278, 278, 278, 277,
    278, 278, 278, 278, 278, 278, 278,
};
static const unsigned short k_c_left[30] = {
    181, 181, 181, 181, 181, 181, 181, 181, 180, 180, 179, 179, 179, 179,
    178, 178, 178, 178, 178, 177, 177,
    178, 178, 178, 178, 178, 178, 178, 178, 178,
};

/* PC SETTING in a 304 x 32 picture (the label's 300 x 30 and a margin):
 * P (lifted a line: GUIDE MAP sits one lower in its cell), C on the P's
 * outline, SETTING as far after it as after CONTROLLER, centred like it. */
static const piece k_label[] = {
    {  30, 30,   0, 299, -197, -1, k_p_left },
    {  60, 30,   0, 199,  -81,  0, k_c_left },
    { 210, 30, 171, 281,  -56,  0, NULL },
};

/* "PC version settings": P of Plays, C of Choose, v of voice, er of
 * Controller, si of music, on, settings; a column between letters, the
 * hints' own word space. Centred in the line. */
static const piece k_hint_en[] = {
    { 256, 32,  93, 105,  61, 0, NULL },
    { 352, 32,  93, 107,  75, 0, NULL },
    { 160, 32, 208, 219, -19, 0, NULL },
    { 192, 32, 249, 270, -48, 0, NULL },
    {  64, 32, 237, 254, -13, 0, NULL },
    { 256, 32, 293, 318, -51, 0, NULL },
    { 320, 32, 296, 380, -23, 0, NULL },
};
/* 設定画面へ, the end of コントローラー設定画面へ, centred. */
static const piece k_hint_ja[] = {
    { 192, 32, 264, 364, -58, 0, NULL },
};

static int empty_rows(const px *img, int w, int y0, int y1)
{
    int x, y;
    for (y = y0; y < y1; y++)
        for (x = 0; x < w; x++)
            if (img[(size_t)y * w + x].a)
                return 0;
    return 1;
}

static int draw_pictures(uint32_t tex0, uint32_t tex1)
{
    px *a = dxt_decode(GP(tex0), 1024, 512, 5);
    px *b = dxt_decode(GP(tex1), 512, 512, 1);
    px *label = calloc(304 * 32, sizeof *label), *hint = calloc(512 * 32, sizeof *hint);
    int ok = 0, ja = 0, i, x, y;

    if (!a || !b || !label || !hint)
        goto out;
    /* The rows the row's pictures and the layout go to must be empty, and
     * the cells they are cut from not. */
    if (!empty_rows(a, 1024, 480, 512) || !empty_rows(b, 512, 384, 512)
            || empty_rows(a, 1024, 210, 240) || empty_rows(b, 512, 192, 224)) {
        fprintf(stderr, LOGP "Options.xpr: its pictures are not the ones expected; no PC SETTING row\n");
        goto out;
    }
    /* Help.tga in Japanese: the controller setting's hint starts further left. */
    for (y = 192; y < 224 && !ja; y++)
        for (x = 148; x < 162; x++)
            if (b[(size_t)y * 512 + x].a)
                ja = 1;
    for (i = 0; i < (int)(sizeof k_label / sizeof k_label[0]); i++)
        put(label, 304, 32, a, 1024, &k_label[i]);
    if (ja)
        put(hint, 512, 32, b, 512, &k_hint_ja[0]);
    else
        for (i = 0; i < (int)(sizeof k_hint_en / sizeof k_hint_en[0]); i++)
            put(hint, 512, 32, b, 512, &k_hint_en[i]);
    dxt_encode(label, 304, 32, GP(tex0), 1024, 0, 480 / 4, 5);
    dxt_encode(hint, 512, 32, GP(tex1), 512, 0, 384 / 4, 1);
    ok = ja ? 2 : 1;
out:
    free(a);
    free(b);
    free(label);
    free(hint);
    return ok;
}

/* ── the layout ────────────────────────────────────────────────────── */

static const char *name_at(uint32_t pool, uint32_t off)
{
    return (const char *)GP(pool + off);
}

static int name_is(uint32_t pool, uint32_t off, const char *want)
{
    return off < 0x1000 && !strcmp(name_at(pool, off), want);
}

/* The end of the pool's last name, from every name the layout uses. */
static uint32_t pool_size(uint32_t L, uint32_t P)
{
    uint32_t pool = L + rd32(L + 0x2C), end = 0, i, n, o;
#define SEE(off) do { uint32_t e_ = (off) + (uint32_t)strlen(name_at(pool, off)) + 1; if (e_ > end) end = e_; } while (0)
    for (i = 0, n = rd32(L + 4), o = L + rd32(L + 8); i < n; i++)
        SEE(rd32(o + 8 * i));
    for (i = 0, n = rd32(L + 0x1C), o = L + rd32(L + 0x20); i < n; i++)
        SEE(rd32(o + 8 * i));
    SEE(rd32(P));
    for (i = 0, n = rd32(P + 4), o = P + rd32(P + 8); i < n; i++)
        SEE(rd32(o + 36 * i));
    for (i = 0, n = rd32(P + 0x14), o = P + rd32(P + 0x18); i < n; i++)
        SEE(rd32(o + 12 * i));
#undef SEE
    return end;
}

/* Copy a track to `at`, its element (move, colour) or animation (start,
 * stop) renumbered; its size. */
static uint32_t copy_track(uint32_t at, uint32_t src)
{
    uint32_t type = rd32(src), size = rd32(src + 4), v;

    memcpy(GP(at), GP(src), size);
    v = rd32(at + 8);
    if ((type == 3 || type == 6) && size >= 12)
        wr32(at + 8, v == E_CTRL ? E_PC : v == E_CTRL + 1 ? E_PC + 1 : v);
    else if ((type == 8 || type == 9) && size >= 12)
        wr32(at + 8, v == A_CTRL + 1 ? A_PC + 1 : v);
    return (size + 3) & ~3u;
}

/* The layout at L checked; its page in *page. */
static int layout_is_options(uint32_t L, uint32_t *page)
{
    uint32_t pool, P, E, A;
    static const char *const elems[] = { "ControllerSetting", "ControllerSetting_Setumei",
                                         "ResetDefault", "ResetDefault_Setumei", "Exit", "Exit_Setumei" };
    static const char *const anims[] = { "Sel_ControllerSetting", "Flash_ControllerSetting",
                                         "UnSel_ControllerSetting" };
    int i;

    /* The counts first, and offsets inside a block of this size, before
     * anything is read through them: every screen's resource comes here. */
    if (rd32(L + 4) != 2 || rd32(L + 0xC) != N_SPR || rd32(L + 0x24) != 1
            || rd32(L + 0x28) >= 0x4000 || rd32(L + 0x2C) >= 0x4000 || rd32(L + rd32(L + 0x28)) >= 0x4000)
        return 0;
    pool = L + rd32(L + 0x2C);
    P = L + rd32(L + rd32(L + 0x28));
    if (rd32(P) >= 0x1000 || rd32(P + 8) >= 0x4000 || rd32(P + 0x18) >= 0x4000)
        return 0;
    if (!name_is(pool, rd32(P), "OptionPage") || rd32(P + 4) != N_ELEM || rd32(P + 0xC) != 0
            || rd32(P + 0x14) != N_ANIM)
        return 0;
    E = P + rd32(P + 8);
    A = P + rd32(P + 0x18);
    for (i = 0; i < 6; i++)
        if (!name_is(pool, rd32(E + 36 * (E_CTRL + i)), elems[i]))
            return 0;
    for (i = 0; i < 3; i++)
        if (!name_is(pool, rd32(A + 12 * (A_CTRL + i)), anims[i]))
            return 0;
    /* The rows' items and links as this code knows them. */
    if ((rd32(E + 36 * E_CTRL + 20) & 0xFFFF) != 6 || (rd32(E + 36 * E_CTRL + 24) & 0xFFFF) != 2
            || rd32(E + 36 * E_RESET + 20) != 0xFFFE0008u)
        return 0;
    *page = P;
    return 1;
}

/* Build the layout with the row at D; its size, or 0 when it does not fit
 * in `room`. */
static uint32_t build_layout(uint32_t L, uint32_t P0, uint32_t D, uint32_t room)
{
    static const char k_names[] = "PcSetting\0PcSetting_Setumei\0Sel_PcSetting\0Flash_PcSetting\0UnSel_PcSetting";
    const uint32_t pool_off = rd32(L + 0x2C), psize = pool_size(L, P0);
    const uint32_t p0 = P0 - L;                          /* the old page, as an offset */
    const uint32_t e0 = p0 + rd32(P0 + 8), a0 = p0 + rd32(P0 + 0x18);
    uint32_t at, spr, page, elem, anim, start_list, pool, nm[5], i, k, y_pc;
    uint32_t start_new[2];

    if (pool_off + psize + 0x2000 > room)
        return 0;
    /* Everything before the pool as it was: the tracks and their lists stay
     * where they are, at the same offsets. */
    memcpy(GP(D), GP(L), pool_off);
    at = (pool_off + 3) & ~3u;

    /* The pictures, and the two new ones. */
    spr = at;
    memcpy(GP(D + spr), GP(L + rd32(L + 0x10)), N_SPR * 24);
    {
        static const uint32_t k_new[2][6] = {
            { 0, 0u | 3840u << 16, 1200u | 4080u << 16, 300u | 30u << 16, 0, 1 },
            { 1, 0u | 3072u << 16, 4096u | 3328u << 16, 512u | 32u << 16, 0, 1 },
        };
        for (i = 0; i < 2; i++)
            for (k = 0; k < 6; k++)
                wr32(D + spr + 24 * (N_SPR + i) + 4 * k, k_new[i][k]);
    }
    wr32(D + 0x0C, N_SPR + 2);
    wr32(D + 0x10, spr);
    at = spr + 24 * (N_SPR + 2);

    /* The page: its header, then 48 elements, then 31 animations. */
    page = at;
    memcpy(GP(D + page), GP(L + p0), 0x20);
    elem = page + 0x20;
    anim = elem + 36 * (N_ELEM + 2);
    wr32(D + page + 4, N_ELEM + 2);
    wr32(D + page + 8, elem - page);
    wr32(D + page + 0x14, N_ANIM + 3);
    wr32(D + page + 0x18, anim - page);
    wr32(D + rd32(L + 0x28), page);                    /* the pages list: this one */
    at = anim + 12 * (N_ANIM + 3);

    /* The string pool last, the new names after the old. */
    for (i = 0, k = 0; i < 5; i++) {
        nm[i] = psize + k;
        k += (uint32_t)strlen(k_names + k) + 1;
    }

    /* The elements, the rows below CONTROLLER SETTING one row down, its
     * down link and RESET TO DEFAULT's up link to the new row. */
    memcpy(GP(D + elem), GP(L + e0), 36 * N_ELEM);
    for (i = 0; i < N_ELEM; i++) {
        uint32_t w3 = rd32(D + elem + 36 * i + 12);
        wr32(D + elem + 36 * i + 12, (w3 & 0xFFFF0000u) | remap_y(w3 & 0xFFFF));
    }
    y_pc = ROW_Y0 + ROW_NEW * PC_ROW;
    {
        uint32_t c = D + elem + 36 * E_CTRL, r = D + elem + 36 * E_RESET;
        uint32_t n = D + elem + 36 * E_PC, h = D + elem + 36 * (E_PC + 1);
        wr32(c + 24, (rd32(c + 24) & 0xFFFF0000u) | (uint16_t)(E_PC - E_CTRL));
        wr32(r + 20, (uint32_t)(uint16_t)(E_PC - E_RESET) << 16 | (rd32(r + 20) & 0xFFFF));
        memcpy(GP(n), GP(c), 36);
        wr32(n + 0, nm[0]);
        wr32(n + 4, (uint32_t)SPR_LABEL << 16);
        wr32(n + 12, (rd32(c + 12) & 0xFFFF0000u) | y_pc);
        wr32(n + 20, (uint32_t)(uint16_t)(E_CTRL - E_PC) << 16 | DS2_OPTIONS_PC_ITEM);
        wr32(n + 24, (rd32(L + e0 + 36 * E_CTRL + 24) & 0xFFFF0000u) | (uint16_t)(E_RESET - E_PC));
        wr32(n + 32, (uint32_t)(A_PC + 2) << 16 | A_PC);
        memcpy(GP(h), GP(c + 36), 36);
        wr32(h + 0, nm[1]);
        wr32(h + 4, (uint32_t)SPR_HINT << 16);
    }

    /* The old animations at their new records: each list's entries (from
     * the record) follow the record; the rows' moves follow their rows. */
    for (i = 0; i < N_ANIM; i++) {
        uint32_t ro = a0 + 12 * i, rn = anim + 12 * i;
        uint32_t nt = rd32(L + ro + 4), list = ro + rd32(L + ro + 8);
        wr32(D + rn, rd32(L + ro));
        wr32(D + rn + 4, nt);
        wr32(D + rn + 8, list - rn);
        for (k = 0; k < nt; k++) {
            uint32_t t = ro + rd32(L + list + 4 * k);   /* the track, as an offset */
            wr32(D + list + 4 * k, t - rn);
            if (rd32(D + t) == 3) {
                uint32_t f = rd32(D + t + 24), to = rd32(D + t + 28);
                wr32(D + t + 24, remap_y(f >> 16) << 16 | (f & 0xFFFF));
                wr32(D + t + 28, remap_y(to >> 16) << 16 | (to & 0xFFFF));
            }
            /* Start: RESET TO DEFAULT and EXIT come in one step later. */
            if (i == 0 && (rd32(D + t) == 3 || rd32(D + t) == 6)
                    && (rd32(D + t + 8) == E_RESET || rd32(D + t + 8) == E_EXIT))
                wr32(D + t + 20, rd32(D + t + 20) + 5);
        }
    }

    /* Start's list again, with the row's slide and fade (CONTROLLER
     * SETTING's, ending where RESET TO DEFAULT's did) before its end. */
    {
        uint32_t ro = a0, rn = anim, nt = rd32(L + ro + 4), list = ro + rd32(L + ro + 8);
        uint32_t last = ro + rd32(L + list + 4 * (nt - 1)), n = 0;

        for (k = 0; k < nt; k++) {
            uint32_t t = ro + rd32(L + list + 4 * k);
            if ((rd32(D + t) == 3 || rd32(D + t) == 6) && rd32(D + t + 8) == E_CTRL && n < 2) {
                start_new[n] = at;
                memcpy(GP(D + at), GP(D + t), 32);
                wr32(D + at + 8, E_PC);
                wr32(D + at + 20, rd32(D + t + 20) + 5);
                if (rd32(D + at) == 3) {
                    wr32(D + at + 24, y_pc << 16 | (rd32(D + at + 24) & 0xFFFF));
                    wr32(D + at + 28, y_pc << 16 | (rd32(D + at + 28) & 0xFFFF));
                }
                at += 32;
                n++;
            }
        }
        if (n != 2 || rd32(D + last) != 0)
            return 0;
        start_list = at;
        for (k = 0; k + 1 < nt; k++)
            wr32(D + start_list + 4 * k, rd32(D + list + 4 * k));
        wr32(D + start_list + 4 * (nt - 1), start_new[0] - rn);
        wr32(D + start_list + 4 * nt, start_new[1] - rn);
        wr32(D + start_list + 4 * (nt + 1), last - rn);
        wr32(D + rn + 4, nt + 2);
        wr32(D + rn + 8, start_list - rn);
        at = start_list + 4 * (nt + 2);
    }

    /* Sel_, Flash_ and UnSel_PcSetting: CONTROLLER SETTING's three, on the
     * new elements and starting and stopping the new Flash_. */
    for (i = 0; i < 3; i++) {
        uint32_t ro = a0 + 12 * (A_CTRL + i), rn = anim + 12 * (A_PC + i);
        uint32_t nt = rd32(L + ro + 4), list = ro + rd32(L + ro + 8), nl = at;
        wr32(D + rn, nm[2 + i]);
        wr32(D + rn + 4, nt);
        wr32(D + rn + 8, nl - rn);
        at = nl + 4 * nt;
        for (k = 0; k < nt; k++) {
            uint32_t t = ro + rd32(L + list + 4 * k);
            wr32(D + nl + 4 * k, at - rn);
            at += copy_track(D + at, L + t);
        }
    }

    pool = at;
    if (pool + psize + sizeof k_names > room)
        return 0;
    memcpy(GP(D + pool), GP(L + pool_off), psize);
    memcpy(GP(D + pool + psize), k_names, sizeof k_names);
    wr32(D + 0x2C, pool);
    return pool + psize + (uint32_t)sizeof k_names;
}

int ds2_options_row_patch(uint32_t res)
{
    uint32_t xpr, list, L, t0, t1, tex0, tex1, P, D, size;
    int art;

    if (GP(res)[0x14] == 1 || rd32(res + 0x10) < 3)
        return 0;
    xpr = rd32(res + 4);
    list = rd32(xpr + 0x3C);
    /* Resource 0 the layout: the loader's sizes at +0x40 and types at
     * +0x44 (user data is 0x80000000), as sub_000EB7A0 checks the size. */
    if (!list || !rd32(xpr + 0x40) || !rd32(xpr + 0x44)
            || rd32(rd32(xpr + 0x44)) != 0x80000000u
            || rd32(rd32(xpr + 0x40)) < 0x30 || rd32(rd32(xpr + 0x40)) > 0x4000)
        return 0;
    L = rd32(list);
    t0 = rd32(list + 4);
    t1 = rd32(list + 8);
    if (!L || !t0 || !t1 || !layout_is_options(L, &P))
        return 0;
    if (rd32(t0 + 0xC) != TEX0_FMT || rd32(t1 + 0xC) != TEX1_FMT) {
        fprintf(stderr, LOGP "Options.xpr: textures not as expected; no PC SETTING row\n");
        return 0;
    }
    /* A registered texture's data is its physical address (D3D keeps VA &
     * 0x0FFFFFFF for the GPU); the CPU sees it at 0x80000000 up. */
    tex0 = rd32(t0 + 4);
    tex1 = rd32(t1 + 4);
    if (tex0 < 0x10000000u)
        tex0 |= 0x80000000u;
    if (tex1 < 0x10000000u)
        tex1 |= 0x80000000u;
    art = draw_pictures(tex0, tex1);
    if (!art)
        return 0;
    /* After the hint's rows and a blank block row (so that filtering at the
     * hint's lower edge reads nothing of it): rows 420 to 511 of Help.tga. */
    D = tex1 + 105 * 128 * 8;
    size = build_layout(L, P, D, 23 * 128 * 8);
    if (!size) {
        fprintf(stderr, LOGP "Options.xpr: the layout did not fit; no PC SETTING row\n");
        return 0;
    }
    wr32(list, D);
    fprintf(stderr, LOGP "OPTIONS: PC SETTING row added (%s hint; layout %u bytes at 0x%08X)\n",
            art == 2 ? "Japanese" : "English", size, D);
    return 1;
}
