/*
 * settings.h - this title's side of the runtime's settings overlay
 * (settings.c). Without the overlay (XBOXRECOMP_OVERLAY off) these do
 * nothing and there is no PC SETTING row.
 */
#ifndef DS2_SETTINGS_H
#define DS2_SETTINGS_H

/* Before the title's runtime defaults: the rows of its own, settings.toml
 * in `save_dir` read and applied. */
void ds2_settings_init(const char *save_dir);
/* A runtime default of the title's (main.c's env_default): also the
 * setting's default in the overlay. */
void ds2_settings_default(const char *name, const char *value);
/* After the enhancements layer read enhance.toml: its values become the
 * defaults, and the settings take what the environment holds. */
void ds2_settings_finish(void);

/* The OPTIONS screen gets its PC SETTING row (an overlay to open, and the
 * overlay's "Entry in the game's menu" on). */
int ds2_pc_setting_row(void);
/* The row was chosen: open the overlay. */
void ds2_pc_setting_open(void);

#endif
