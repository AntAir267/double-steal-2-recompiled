/*
 * skin.h - the settings overlay in the game's own look (skin.c).
 */
#ifndef DS2_SKIN_H
#define DS2_SKIN_H

/* Reads the OPTIONS screen from the save folder's Cache and the text font
 * from the game folder, and gives the overlay the game's look. Without the
 * cache (the game unpacks it on its first start) it stays the overlay's own
 * until ds2_skin_retry finds it. */
void ds2_skin_init(const char *game_dir, const char *save_dir);
/* Again, if the first time found no cache (before the overlay opens). */
void ds2_skin_retry(void);

#endif
