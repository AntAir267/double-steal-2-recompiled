/*
 * settings.c - this title's side of the runtime's settings overlay, which
 * OPTIONS > PC SETTING opens (options_row.c), as do Back + Start and F1.
 *
 * The overlay keeps what the player changes in settings.toml in the save
 * folder. enhance.toml beside the executable stays this build's defaults:
 * its values are the overlay's defaults (Reset goes back to them), and
 * whatever settings.toml sets is put in the environment first, which every
 * reader of a setting, the enhancements layer included, ranks over the file.
 *
 * The title's own rows: the dashboard's video mode, the frame's picture
 * area (the HD picture inside the TV-safe area), the language, the logo
 * movie and the engine's hum on the controller. Hidden: the 2D and texture upscalers, the overscan crop (the
 * picture area does it right for this game) and the button prompt sets
 * (the game's prompts are its own pictures, not a sheet the runtime swaps).
 */
#include "settings.h"
#include "skin.h"

#include <stdio.h>
#include <stdlib.h>

#if defined(XBOXRECOMP_HAVE_OVERLAY)

#include "xbox_settings.h"
#include "platform/recomp_env.h"
#include "enhance_cfg.h"
#include "recomp_cfg.h"
#include "nv2a_backend_common.h"    /* nv2a_present_set_crop */

extern int ds2_skip_intro;          /* main.c */
extern int ds2_engine_vibration;

static void crop_apply(int v) { nv2a_present_set_crop(v); }
static void intro_apply(int v) { ds2_skip_intro = v; }
static void engine_apply(int v) { ds2_engine_vibration = v; }

static const char *const k_hdtv[] = { "off", "480p", "720p", NULL };
static const char *const k_hdtv_names[] = { "480i (the console's own)", "480p", "720p (the game's HD mode)", NULL };
static const char *const k_crop[] = { "off", "clip", NULL };
static const char *const k_crop_names[] = { "The whole frame", "The game's picture", NULL };
static const char *const k_lang[] = { "english", "japanese", NULL };
static const char *const k_lang_names[] = { "English", "Japanese", NULL };

static const xbox_title_setting k_title[] = {
    { "display.hdtv", RENV_DISPLAY_HDTV, k_hdtv, k_hdtv_names, "Video mode",
      "The Xbox dashboard's video mode, which the game reads when it starts. In 720p it draws"
      " its own HD picture, 1280x720 (16:9 with the aspect ratio at 16:9); 480i and 480p are"
      " the console's 640x480.",
      XS_PAGE_DISPLAY, 1, NULL },
    { "display.crop", RENV_PRESENT_CROP, k_crop, k_crop_names, "Picture area",
      "In 720p the game keeps its picture inside the TV-safe area, with a border round it that a"
      " TV's overscan hid. The game's picture shows only that part, edge to edge; the whole"
      " frame shows the border too. Changes at once.",
      XS_PAGE_DISPLAY, 0, crop_apply },
    { "system.language", RENV_LANGUAGE, k_lang, k_lang_names, "Language",
      "The dashboard's language, which the game reads when it starts: its menus and text in"
      " English or Japanese. The first start in a language copies that language's files to"
      " the hard disk cache again.",
      XS_PAGE_DISPLAY, 1, NULL },
    { "game.skip_intro", RENV_SKIP_INTRO, NULL, NULL, "Skip the logo movie",
      "Starts at the title screen, without the Bunkasha logo movie. Applies from the next start.",
      XS_PAGE_DISPLAY, 0, intro_apply },
    { "game.engine_vibration", RENV_ENGINE_VIBRATION, NULL, NULL, "Engine vibration",
      "The game hums the controller with the engine's load whenever you accelerate, up to a third"
      " of full on both motors. On is the console's own; off keeps only the knocks: collisions,"
      " rough road and the rev limiter. Changes at once.",
      XS_PAGE_INPUT, 0, engine_apply },
};

/* enhance.toml's keys the overlay has a row for, by the variable it sets. */
static const struct { const char *key; recomp_env_id env; } k_file_keys[] = {
    { "display.hdtv", RENV_DISPLAY_HDTV },
    { "display.widescreen", RENV_DISPLAY_WIDESCREEN },
    { "render.scale", RENV_RENDER_SCALE },
    { "graphics.aniso", RENV_GFX_ANISO },
    { "graphics.aa", RENV_GFX_AA },
    { "present.crop", RENV_PRESENT_CROP },
    { "present.filter", RENV_PRESENT_FILTER },
    { "present.fullscreen", RENV_PRESENT_FULLSCREEN },
    { "system.language", RENV_LANGUAGE },
    { "game.skip_intro", RENV_SKIP_INTRO },
    { "game.engine_vibration", RENV_ENGINE_VIBRATION },
};

void ds2_settings_init(const char *save_dir)
{
    xbox_SettingsAddTitle(k_title, (int)(sizeof k_title / sizeof k_title[0]));
    xbox_SettingsHide(XS_GRAPHICS_UI_SCALE);
    xbox_SettingsHide(XS_GRAPHICS_TEXUP);
    xbox_SettingsHide(XS_GRAPHICS_TEXUP_SCALE);
    xbox_SettingsHide(XS_GRAPHICS_TEXUP_ROADS);
    xbox_SettingsHide(XS_GRAPHICS_TEXUP_PRELOAD);
    xbox_SettingsHide(XS_DISPLAY_OVERSCAN);
    xbox_SettingsHide(XS_INPUT_GLYPHS);
    xbox_SettingsSetMenuEntryHint(
        "Adds PC SETTING to the game's OPTIONS screen (from the main menu and the pause menu)."
        " It opens this menu, like Back + Start or F1. Shows the next time that screen opens.");
    /* settings.toml beside the saves (--save-dir), where recomp_save_dir
     * looks first. */
    setenv("RECOMP_SAVE_DIR", save_dir, 0);
    xbox_SettingsInit("doublesteal2");
}

void ds2_settings_default(const char *name, const char *value)
{
    xbox_SettingsGameDefault(name, value);
}

void ds2_settings_finish(void)
{
    size_t i;

    for (i = 0; i < sizeof k_file_keys / sizeof k_file_keys[0]; i++) {
        const char *key = k_file_keys[i].key;
        const recomp_cfg *c = enhance_cfg_lookup(key);
        char v[64];
        if (!c)
            continue;
        switch (recomp_cfg_type_of(c, key)) {
        case RCFG_STRING: snprintf(v, sizeof v, "%s", recomp_cfg_string(c, key, "")); break;
        case RCFG_INT:    snprintf(v, sizeof v, "%lld", recomp_cfg_int(c, key, 0)); break;
        case RCFG_FLOAT:  snprintf(v, sizeof v, "%g", recomp_cfg_float(c, key, 0)); break;
        case RCFG_BOOL:   snprintf(v, sizeof v, "%d", recomp_cfg_bool(c, key, 0)); break;
        default:          continue;
        }
        /* As an env_default: the variable, under what the environment and
         * settings.toml set, and the overlay told. */
        setenv(recomp_env_name(k_file_keys[i].env), v, 0);
        xbox_SettingsGameDefault(recomp_env_name(k_file_keys[i].env), v);
    }
    xbox_SettingsFinish();
    xbox_OverlaySetAbout("Double-S.T.E.A.L: The Second Clash (Xbox, 2004), recompiled."
                         " Settings: OPTIONS > PC SETTING, Back + Start or F1.");
}

int ds2_pc_setting_row(void)
{
    const char *e = recomp_env(RENV_MENU_ENTRY);
    return xbox_OverlayAvailable() && !(e && e[0] == '0');
}

void ds2_pc_setting_open(void)
{
    ds2_skin_retry();               /* the first start: the cache is there by now */
    xbox_OverlayRequestOpen();
}

#else

void ds2_settings_init(const char *save_dir) { (void)save_dir; }
void ds2_settings_default(const char *name, const char *value) { (void)name; (void)value; }
void ds2_settings_finish(void) {}
int ds2_pc_setting_row(void) { return 0; }
void ds2_pc_setting_open(void) {}

#endif
