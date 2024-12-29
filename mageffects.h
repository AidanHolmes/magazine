#ifndef ___H_MAGEFFECTS_
#define ___H_MAGEFFECTS_
#include "magui.h"
#include "ptplayer.h"

extern UBYTE mt_SongEnd; // not included in ptplayer header, but exportable from asm

BOOL effectsInit(struct MagUIData *uidata, char *szEffect);

BOOL initModMusic(struct MagUIData *uidata);
void cleanupModMusic(struct MagUIData *uidata);
void stopModMusic(struct MagUIData *uidata);
BOOL restartModMusic(struct MagUIData *uidata);
BOOL startModMusic(struct MagUIData *uidata, struct IFFMod *mod);
void musicTickEvent(struct MagUIData *uidata);




















#endif