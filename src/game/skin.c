/*
 * skin.c - the settings overlay in Double-S.T.E.A.L 2's own look
 * (xbox_OverlaySetSkin): the frame, the row plates, the arrows and the A / B
 * prompts of its OPTIONS screen, a PC SETTING heading cut from its own
 * headings (P from OPTIONS, C and SETTING from CONTROLLER SETTING), the
 * colours its menus use, and its text font, data/fonts/arial32.bmf (an
 * XFONT bitmap font), for the words.
 *
 * Everything comes from the player's files at run time: the screens from
 * the cache the game unpacks on its first start (the save folder's
 * Cache/data/screen), the font from the game folder. Nothing is written.
 * Each screen's art is checked by a hash of the texture blocks used (the
 * same in the English and the Japanese packs); what does not match is left
 * out, and without the OPTIONS screen there is no skin (the overlay keeps
 * its own look).
 */
#include "skin.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(XBOXRECOMP_HAVE_OVERLAY)
#include "xbox_settings.h"
#endif

/* A toolkit without the skin API (antbox before it) builds the stubs. */
#if defined(XBOX_OVERLAY_HAS_SKIN)

#include "options_row.h"            /* ds2_dxt_to_rgba */

static char s_game[1024], s_save[1024];
static int s_done;

/* ── files ─────────────────────────────────────────────────────────── */

static uint8_t *read_file(const char *path, size_t *n)
{
    FILE *f = fopen(path, "rb");
    uint8_t *d = NULL;
    long len;

    if (!f)
        return NULL;
    if (fseek(f, 0, SEEK_END) == 0 && (len = ftell(f)) > 0 && len < (64L << 20) && fseek(f, 0, SEEK_SET) == 0
        && (d = malloc((size_t)len)) && fread(d, 1, (size_t)len, f) != (size_t)len) {
        free(d);
        d = NULL;
    }
    if (d)
        *n = (size_t)len;
    fclose(f);
    return d;
}

static uint32_t le32(const uint8_t *p) { return (uint32_t)p[0] | p[1] << 8 | p[2] << 16 | (uint32_t)p[3] << 24; }

/* The first texture of an XPR0 file (the screens' 1024 x 512 DXT5 one, after
 * the layout): its blocks, or NULL when it is not that. */
static const uint8_t *xpr_tex0(const uint8_t *d, size_t n)
{
    uint32_t hs, o = 12;

    if (n < 0x40 || memcmp(d, "XPR0", 4) || (hs = le32(d + 8)) > n)
        return NULL;
    while (o + 20 <= hs && (le32(d + o) & 0x80000000u))
        o += 8 + le32(d + o + 4);
    if (o + 20 > hs || (le32(d + o) >> 16 & 7) != 4 || le32(d + o + 12) != 0x09A10F29u
        || (uint64_t)hs + le32(d + o + 4) + 1024 * 512 > n)
        return NULL;
    return d + hs + le32(d + o + 4);
}

/* FNV-1a (64) of the DXT5 blocks of [x0, x1) x [y0, y1) of a 1024-wide texture. */
static uint64_t blocks_hash(const uint8_t *tex, int x0, int y0, int x1, int y1)
{
    uint64_t h = 0xcbf29ce484222325ull;
    int bx, by, i;

    for (by = y0 / 4; by < (y1 + 3) / 4; by++)
        for (bx = x0 / 4; bx < (x1 + 3) / 4; bx++)
            for (i = 0; i < 16; i++) {
                h ^= tex[((size_t)by * 256 + bx) * 16 + i];
                h *= 0x100000001b3ull;
            }
    return h;
}

/* ── pictures ──────────────────────────────────────────────────────── */

typedef struct { uint8_t *rgba; int w, h; } pic;

/* [x0, x1) x [y0, y1) of a 1024 x 512 picture. */
static pic crop(const uint8_t *img, int x0, int y0, int x1, int y1)
{
    pic p = { malloc((size_t)(x1 - x0) * (y1 - y0) * 4), x1 - x0, y1 - y0 };
    int y;

    if (p.rgba)
        for (y = y0; y < y1; y++)
            memcpy(p.rgba + (size_t)(y - y0) * p.w * 4, img + ((size_t)y * 1024 + x0) * 4, (size_t)p.w * 4);
    return p;
}

/* A sprite's cell cut to what is drawn in it (alpha), and where that is in
 * the cell. */
static pic crop_drawn(const uint8_t *img, int x0, int y0, int x1, int y1, int *ox, int *oy)
{
    int x, y, a0 = x1, b0 = y1, a1 = x0, b1 = y0;

    for (y = y0; y < y1; y++)
        for (x = x0; x < x1; x++)
            if (img[((size_t)y * 1024 + x) * 4 + 3]) {
                if (x < a0) a0 = x;
                if (x >= a1) a1 = x + 1;
                if (y < b0) b0 = y;
                if (y >= b1) b1 = y + 1;
            }
    if (a1 <= a0) {
        pic none = { NULL, 0, 0 };
        return none;
    }
    *ox = a0 - x0;
    *oy = b0 - y0;
    return crop(img, a0, b0, a1, b1);
}

/* The headings are leant a quarter of a texel a row: a letter is the
 * columns u in [l, r) of a strip, u = x - (h - 1 - y) / 4 (y from its top). */
#define SLANT 0.25f
typedef struct { const uint8_t *img; int y0; float l, r, extra; } letters;

/* The pieces side by side, two texels apart (and `extra` more before a
 * piece), on a strip 32 high. */
static pic heading(const letters *pc, int n)
{
    const int h = 32;
    float x = 0, width = 0;
    int i, k, sx, y;
    pic p;

    for (i = 0; i < n; i++)
        width += pc[i].extra + (pc[i].r - pc[i].l) + 2.0f;
    p.w = (int)(width + SLANT * h) + 2;
    p.h = h;
    p.rgba = calloc((size_t)p.w * h, 4);
    if (!p.rgba)
        return p;
    for (i = 0; i < n; i++) {
        x += pc[i].extra;
        for (y = 0; y < h; y++)
            for (sx = 0; sx < 380; sx++) {
                const float u = sx - SLANT * (h - 1 - y);
                const uint8_t *s = pc[i].img + ((size_t)(pc[i].y0 + y) * 1024 + sx) * 4;
                int dx;
                if (u < pc[i].l || u >= pc[i].r || !s[3])
                    continue;
                dx = (int)(x + (sx - pc[i].l));
                if (dx < 0 || dx >= p.w)
                    continue;
                for (k = 0; k < 4; k++)
                    p.rgba[((size_t)y * p.w + dx) * 4 + k] = s[k];
            }
        x += (pc[i].r - pc[i].l) + 2.0f;
    }
    return p;
}

/* ── the font (data/fonts/arial32.bmf) ─────────────────────────────── */

/* An XFONT bitmap font [verified on arial32.bmf and xfont.bmf; the
 * character lookup is the game's sub_00118728]: 'TN', a version, 8 header
 * bytes (0x0A the ascent, 0x0C 4 = anti-aliased), u32 glyph offsets from
 * 0x10 (one more than the glyphs, the last the file's size), then the
 * character map: ranges (first 16-code chunk, chunks, first pair: u16s)
 * and per chunk the first glyph and a mask of the codes present (u16s). A
 * glyph: height, width, advance, x offset (s8), top above the baseline
 * (s8), then nibble runs, low nibble first: clear and solid alternate from
 * clear, 15 adds the next nibble; anti-aliased, 0 starts an escape: 0 0 is
 * an empty run (the colour changes), 0 a one pixel of alpha a / 15. */
typedef struct {
    const uint8_t *d;
    size_t n;
    uint32_t nglyph, ranges, nranges, pairs;
    int aa, ascent;
} bmf;

static int bmf_open(bmf *f, const uint8_t *d, size_t n)
{
    uint32_t i, o, np = 0;

    memset(f, 0, sizeof *f);
    if (n < 0x20 || d[0] != 'T' || d[1] != 'N')
        return 0;
    f->d = d;
    f->n = n;
    f->aa = d[0x0C] != 0;
    f->ascent = d[0x0A];
    for (i = 0; 0x10 + 4 * (size_t)i + 4 <= n; i++)
        if (le32(d + 0x10 + 4 * i) == n)
            break;
    if (0x10 + 4 * (size_t)i + 4 > n || i < 95)
        return 0;
    f->nglyph = i;
    f->ranges = o = 0x10 + 4 * (i + 1);
    for (;; o += 6) {
        if (o + 6 > n || (uint32_t)(d[o + 4] | d[o + 5] << 8) != np)
            break;
        np += (uint32_t)(d[o + 2] | d[o + 3] << 8);
        f->nranges++;
    }
    f->pairs = o;
    return f->nranges && f->pairs + 4 * (size_t)np <= n;
}

static int bmf_index(const bmf *f, uint32_t code)
{
    uint32_t chunk = code >> 4, bit = code & 15, i;

    for (i = 0; i < f->nranges; i++) {
        const uint8_t *r = f->d + f->ranges + 6 * i;
        uint32_t start = (uint32_t)(r[0] | r[1] << 8), cnt = (uint32_t)(r[2] | r[3] << 8), first = (uint32_t)(r[4] | r[5] << 8);
        if (chunk >= start && chunk < start + cnt) {
            const uint8_t *p = f->d + f->pairs + 4 * (first + chunk - start);
            uint32_t base = (uint32_t)(p[0] | p[1] << 8), mask = (uint32_t)(p[2] | p[3] << 8), below, k = 0;
            if (!(mask >> bit & 1))
                return -1;
            for (below = mask & ((1u << bit) - 1); below; below &= below - 1)
                k++;
            return base + k < f->nglyph ? (int)(base + k) : -1;
        }
    }
    return -1;
}

/* Glyph `i` into `out` (w x h alpha), its header in hdr[5]. 0 when it does
 * not read as one. */
static int bmf_glyph(const bmf *f, int i, uint8_t hdr[5], uint8_t *out, int room)
{
    const uint32_t a = le32(f->d + 0x10 + 4 * (uint32_t)i), b = le32(f->d + 0x14 + 4 * (uint32_t)i);
    const uint8_t *g = f->d + a;
    int n, k = 0, nb, px = 0, st = 0;

    if (b < a + 5 || b > f->n)
        return 0;
    memcpy(hdr, g, 5);
    n = g[0] * g[1];
    if (n > room)
        return 0;
    memset(out, 0, (size_t)n);
    nb = (int)(b - a - 5) * 2;
#define NIB(j) (g[5 + (j) / 2] >> ((j) & 1 ? 4 : 0) & 15)
    while (k < nb && px < n) {
        int v = NIB(k), run;
        k++;
        if (v == 0 && f->aa) {
            int al = k < nb ? NIB(k) : 0;
            k++;
            if (!al)
                st ^= 1;
            else
                out[px++] = (uint8_t)(al * 17);
            continue;
        }
        run = v;
        while (v == 15 && k < nb) {
            v = NIB(k);
            k++;
            run += v;
        }
        for (; run > 0 && px < n; run--)
            out[px++] = st ? 255 : 0;
        st ^= 1;
    }
#undef NIB
    return 1;
}

/* The printable ASCII glyphs into an atlas, each a texel bolder (the game's
 * own lettering is bold Arial; its font file is the regular weight). */
#define ATLAS_W 512
#define ATLAS_H 256
static int font_atlas(const bmf *f, uint8_t *atlas, xbox_overlay_atlas_glyph *gl, uint32_t *count)
{
    uint8_t one[64 * 64], hdr[5];
    int x = 1, y = 1, row = 0;
    uint32_t c, n = 0;

    for (c = 0x20; c < 0x7F; c++) {
        const int i = bmf_index(f, c);
        int gw, gh, xx, yy;
        if (i < 0 || !bmf_glyph(f, i, hdr, one, (int)sizeof one))
            continue;
        gh = hdr[0];
        gw = hdr[1] ? hdr[1] + 1 : 0;
        if (x + gw + 2 > ATLAS_W) {
            x = 1;
            y += row + 2;
            row = 0;
        }
        if (y + gh + 1 > ATLAS_H)
            return 0;
        for (yy = 0; yy < gh; yy++)
            for (xx = 0; xx < gw; xx++) {
                const int l = xx < hdr[1] ? one[yy * hdr[1] + xx] : 0;
                const int r = xx > 0 ? one[yy * hdr[1] + xx - 1] : 0;
                atlas[(size_t)(y + yy) * ATLAS_W + x + xx] = (uint8_t)(l > r ? l : r);
            }
        gl[n].code = c;
        gl[n].x = (uint16_t)x;
        gl[n].y = (uint16_t)y;
        gl[n].w = (uint16_t)gw;
        gl[n].h = (uint16_t)gh;
        gl[n].xo = (int8_t)hdr[3];
        gl[n].yo = (int16_t)(f->ascent - (int8_t)hdr[4]);
        gl[n].adv = (uint16_t)(hdr[2] + 1);
        n++;
        x += gw + 2;
        if (gh > row)
            row = gh;
    }
    *count = n;
    return n >= 90;
}

/* ── the menu sounds (data/sound: System.xsb, InMemory.xwb) ─────────── */

/* The game's menus play System.xsb's cues through 0x0008B960 as 0x2000 | cue
 * [verified: the A handler and sub_0010A530 (left / right, 0x2003); Sound.csv
 * names them]: 0 the title's start, 1 confirm (決定音), 2 cancel, 3 the
 * cursor, 4 an error. The bank (XACT, version 11): its counts at 0x1C, the
 * cues from 0x38 (20 bytes: the sound at +2), the sounds after them (20
 * bytes: the wave at +0, the volume in 1/100 dB at +4) [verified on this
 * file]. The waves are in InMemory.xwb (XACT wave bank 3: bank data, entry
 * metadata of 24 bytes, the wave data; a format with 2 bits of tag, 1 Xbox
 * ADPCM, 3 of channels and 26 of rate), Xbox ADPCM: IMA, a 36-byte block of
 * 65 samples, as the runtime's apu_state.h decodes it. */
static int16_t *ima_decode(const uint8_t *src, uint32_t n, uint32_t *out_n)
{
    static const uint16_t step[89] = {
        7, 8, 9, 10, 11, 12, 13, 14, 16, 17, 19, 21, 23, 25, 28, 31, 34, 37, 41, 45, 50, 55, 60, 66, 73, 80,
        88, 97, 107, 118, 130, 143, 157, 173, 190, 209, 230, 253, 279, 307, 337, 371, 408, 449, 494, 544, 598,
        658, 724, 796, 876, 963, 1060, 1166, 1282, 1411, 1552, 1707, 1878, 2066, 2272, 2499, 2749, 3024, 3327,
        3660, 4026, 4428, 4871, 5358, 5894, 6484, 7132, 7845, 8630, 9493, 10442, 11487, 12635, 13899, 15289,
        16818, 18500, 20350, 22385, 24623, 27086, 29794, 32767 };
    static const int8_t adj[8] = { -1, -1, -1, -1, 2, 4, 6, 8 };
    const uint32_t blocks = n / 36;
    int16_t *pcm = malloc((size_t)blocks * 65 * sizeof *pcm);
    uint32_t b, k = 0;

    if (!pcm || !blocks) {
        free(pcm);
        return NULL;
    }
    for (b = 0; b < blocks; b++) {
        const uint8_t *p = src + 36 * b;
        int pred = (int16_t)(p[0] | p[1] << 8), idx = p[2] > 88 ? 88 : p[2], i;
        pcm[k++] = (int16_t)pred;
        for (i = 4; i < 36; i++) {
            int half;
            for (half = 0; half < 2; half++) {
                const int nib = half ? p[i] >> 4 : p[i] & 15, st = step[idx];
                int diff = st >> 3;
                if (nib & 4) diff += st;
                if (nib & 2) diff += st >> 1;
                if (nib & 1) diff += st >> 2;
                pred += nib & 8 ? -diff : diff;
                pred = pred < -32768 ? -32768 : pred > 32767 ? 32767 : pred;
                idx += adj[nib & 7];
                idx = idx < 0 ? 0 : idx > 88 ? 88 : idx;
                pcm[k++] = (int16_t)pred;
            }
        }
    }
    *out_n = k;
    return pcm;
}

static int s_snd_done;

static int menu_sounds(void)
{
#if defined(XBOX_OVERLAY_HAS_PCM_SOUNDS)
    /* The overlay's cues from the game's: the cursor for a move, a value and
     * a page, confirm for A, cancel for back and close; none for opening
     * (PC SETTING has played confirm already). */
    static const int k_game_cue[XBOX_OVERLAY_SND_COUNT] = {
        [XBOX_OVERLAY_SND_MOVE] = 3, [XBOX_OVERLAY_SND_CHANGE] = 3, [XBOX_OVERLAY_SND_SELECT] = 1,
        [XBOX_OVERLAY_SND_BACK] = 2, [XBOX_OVERLAY_SND_OPEN] = -1, [XBOX_OVERLAY_SND_CLOSE] = 2,
        [XBOX_OVERLAY_SND_PAGE] = 3,
    };
    char path[1200];
    size_t nx = 0, nw = 0;
    uint8_t *xsb, *xwb;
    xbox_overlay_pcm_sound cues[XBOX_OVERLAY_SND_COUNT];
    int16_t *pcm[5] = { 0 };
    uint32_t len[5] = { 0 }, rate[5] = { 0 }, ncue, nsnd, nwave, meta, wave;
    float gain[5] = { 0 };
    int i, made = 0;

    snprintf(path, sizeof path, "%s/Cache/data/sound/System.xsb", s_save);
    xsb = read_file(path, &nx);
    snprintf(path, sizeof path, "%s/Cache/data/sound/InMemory.xwb", s_save);
    xwb = read_file(path, &nw);
    if (!xsb || !xwb || nx < 0x38 || memcmp(xsb, "SDBK", 4) || (xsb[4] | xsb[5] << 8) != 0x0B
        || memcmp(xsb + 0x28, "System", 7) || nw < 0x50 || memcmp(xwb, "WBND", 4) || le32(xwb + 4) != 3)
        goto out;
    ncue = (uint32_t)(xsb[0x1C] | xsb[0x1D] << 8);
    nsnd = (uint32_t)(xsb[0x1E] | xsb[0x1F] << 8);
    meta = le32(xwb + 0x10);
    wave = le32(xwb + 0x20);
    nwave = le32(xwb + le32(xwb + 8) + 4);
    if (ncue != 5 || nsnd != 5 || 0x38 + 20 * (ncue + nsnd) > nx || (uint64_t)meta + 24ull * nwave > nw)
        goto out;
    for (i = 0; i < 5; i++) {
        const uint32_t snd = (uint32_t)(xsb[0x38 + 20 * i + 2] | xsb[0x38 + 20 * i + 3] << 8);
        const uint8_t *so = xsb + 0x38 + 20 * ncue + 20 * snd;
        const uint32_t w = (uint32_t)(so[0] | so[1] << 8);
        const uint8_t *e = xwb + meta + 24 * w;
        const uint32_t fmt = le32(e + 4), off = le32(e + 8), bytes = le32(e + 12);
        if (snd >= nsnd || w >= nwave || (fmt & 3) != 1 || (fmt >> 2 & 7) != 1 || (uint64_t)wave + off + bytes > nw)
            continue;
        pcm[i] = ima_decode(xwb + wave + off, bytes, &len[i]);
        gain[i] = (int16_t)(so[4] | so[5] << 8) / 100.0f;
        rate[i] = fmt >> 5 & 0x3FFFFFF;
    }
    memset(cues, 0, sizeof cues);
    for (i = 0; i < XBOX_OVERLAY_SND_COUNT; i++) {
        const int c = k_game_cue[i];
        if (c < 0 || !pcm[c])
            continue;
        cues[i].pcm = pcm[c];
        cues[i].samples = len[c];
        cues[i].rate = rate[c];
        /* 1.4 dB under the bank's volume: the game's own cursor and confirm
         * reach the output 1.8 and 1.0 dB under it [measured: RMS in a
         * RECOMP_AUDIO_WAV capture, run sk5], the APU's resampling taking
         * some of their top end. */
        cues[i].gain_db = gain[c] - 1.4f;
        made++;
    }
    if (made)
        xbox_OverlaySetMenuSoundsPCM(cues, NULL);
out:
    printf("[DS2] the overlay's menu sounds: %s\n", made ? "the game's" : "none (no sound bank in the cache yet)");
    for (i = 0; i < 5; i++)
        free(pcm[i]);
    free(xwb);
    free(xsb);
    return made > 0;
#else
    return 1;
#endif
}

/* ── the skin ──────────────────────────────────────────────────────── */

/* The art used, by the FNV-1a of its texture blocks (one value: the English
 * and Japanese packs share it) [verified 2026-10-27, both packs]. */
#define OPTIONS_TEX0_HASH 0xbe9aae449a47acf0ull           /* Options.xpr, the whole first texture */
#define CTL_HEADING_HASH  0xd2783ddde754de07ull           /* ControllerSetting.xpr, (0, 420)-(360, 452) */

static void set_pic(xbox_overlay_picture *d, pic p, float x, float y)
{
    d->rgba = p.rgba;
    d->w = (uint32_t)p.w;
    d->h = (uint32_t)p.h;
    d->x = x;
    d->y = y;
}

static int apply(void)
{
    char path[1200];
    size_t n_opt = 0, n_ctl = 0, n_font = 0;
    uint8_t *opt = NULL, *ctl = NULL, *font = NULL, *a = NULL, *c = NULL, *atlas = NULL;
    const uint8_t *t0, *t1 = NULL;
    xbox_overlay_skin k;
    xbox_overlay_atlas_glyph gl[128];
    uint32_t ng = 0;
    pic pics[8];
    int np = 0, ox, oy, ok = 0, i;
    bmf f;

    snprintf(path, sizeof path, "%s/Cache/data/screen/Options.xpr", s_save);
    opt = read_file(path, &n_opt);
    t0 = opt ? xpr_tex0(opt, n_opt) : NULL;
    if (!t0 || blocks_hash(t0, 0, 0, 1024, 512) != OPTIONS_TEX0_HASH || !(a = ds2_dxt_to_rgba(t0, 1024, 512, 5)))
        goto out;
    snprintf(path, sizeof path, "%s/Cache/data/screen/ControllerSetting.xpr", s_save);
    ctl = read_file(path, &n_ctl);
    if (ctl && (t1 = xpr_tex0(ctl, n_ctl)) && blocks_hash(t1, 0, 420, 360, 452) == CTL_HEADING_HASH)
        c = ds2_dxt_to_rgba(t1, 1024, 512, 5);

    memset(&k, 0, sizeof k);
    k.page = (xbox_overlay_rgb){ 0, 0, 0 };
    k.text = (xbox_overlay_rgb){ 111, 111, 111 };      /* the items' 0x808080 over their lettering */
    k.text_hi = (xbox_overlay_rgb){ 60, 255, 0 };      /* the selected item's flash: to 0x3CFF00 */
    k.text_dim = (xbox_overlay_rgb){ 90, 90, 90 };
    k.hint = (xbox_overlay_rgb){ 255, 168, 0 };        /* its value and colon: to 0xFFA800 */
    k.attn = (xbox_overlay_rgb){ 255, 230, 0 };
    k.outline = (xbox_overlay_rgb){ 0, 0, 0 };
    k.outline_w = 0.06f;
    k.lean = 0.0f;
    k.flash_ms = 500;                                  /* 15 frames at the game's 30 */

    /* The screen's backdrop is (384, 0)-(1024, 480): its header, the first
     * row's plate, its footer; the arrows' and prompts' cells are the
     * layout's pictures 1, 2, 30 and 31, the prompts at (35, 420) and (105, 420). */
    pics[np] = crop(a, 384, 0, 1024, 63);
    set_pic(&k.header, pics[np++], 0, 0);
    pics[np] = crop(a, 384, 386, 1024, 480);
    set_pic(&k.footer, pics[np++], 0, 386);
    pics[np] = crop(a, 454, 77, 954, 107);
    set_pic(&k.row, pics[np++], 0, 0);
    pics[np] = crop_drawn(a, 300, 30, 330, 60, &ox, &oy);
    set_pic(&k.arrow_left, pics[np++], 0, 0);
    pics[np] = crop_drawn(a, 300, 0, 330, 30, &ox, &oy);
    set_pic(&k.arrow_right, pics[np++], 0, 0);
    pics[np] = crop_drawn(a, 0, 420, 300, 450, &ox, &oy);
    set_pic(&k.prompt_ok, pics[np++], 35.0f + ox, 420.0f + oy);
    pics[np] = crop_drawn(a, 0, 450, 300, 480, &ox, &oy);
    set_pic(&k.prompt_back, pics[np++], 105.0f + ox, 420.0f + oy);
    if (c) {
        const letters pc[3] = {
            { a, 0, 21.0f, 40.0f, 0.0f },          /* P, from OPTIONS */
            { c, 420, -2.0f, 19.0f, 0.0f },        /* C, from CONTROLLER */
            { c, 420, 215.0f, 351.0f, 9.0f },      /* SETTING */
        };
        pics[np] = heading(pc, 3);
        set_pic(&k.title, pics[np++], 40, 30);  /* where OPTIONS is (the layout's Title) */
    }
    k.tabs_y = 76.0f;
    k.rows_y = 92.0f;
    k.row_pitch = 33.0f;                               /* the screen's own */
    k.hint_y = 403.0f;                                 /* its hints: 30 high from 388 */
    k.prompts_x = k.prompt_ok.x;
    k.prompts_y = 435.0f;
    k.prompts_gap = k.prompt_back.x - (k.prompt_ok.x + (float)k.prompt_ok.w);

    snprintf(path, sizeof path, "%s/data/fonts/arial32.bmf", s_game);
    font = read_file(path, &n_font);
    atlas = calloc(ATLAS_W, ATLAS_H);
    if (font && atlas && bmf_open(&f, font, n_font) && f.aa && font_atlas(&f, atlas, gl, &ng))
        xbox_OverlaySetGameFontAtlas(atlas, ATLAS_W, ATLAS_H, 23, gl, ng);   /* capitals 17 of 23: the size of the hints */
    else
        fprintf(stderr, "[DS2] the overlay keeps its own font (%s)\n", path);

    xbox_OverlaySetSkin(&k);
    printf("[DS2] the overlay in the game's look (%s heading, %u glyphs)\n", c ? "its own" : "no", ng);
    ok = 1;
    for (i = 0; i < np; i++)
        free(pics[i].rgba);
out:
    if (!ok)
        fprintf(stderr, "[DS2] the overlay keeps its own look: no OPTIONS screen in the cache yet\n");
    free(atlas);
    free(font);
    free(c);
    free(a);
    free(ctl);
    free(opt);
    return ok;
}

void ds2_skin_init(const char *game_dir, const char *save_dir)
{
    snprintf(s_game, sizeof s_game, "%s", game_dir);
    snprintf(s_save, sizeof s_save, "%s", save_dir);
    s_done = apply();
    s_snd_done = menu_sounds();
}

void ds2_skin_retry(void)
{
    if (!s_done && s_save[0])
        s_done = apply();
    if (!s_snd_done && s_save[0])
        s_snd_done = menu_sounds();
}

#else

void ds2_skin_init(const char *game_dir, const char *save_dir) { (void)game_dir; (void)save_dir; }
void ds2_skin_retry(void) {}

#endif
