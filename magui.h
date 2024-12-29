#ifndef ___H_MAGUI_
#define ___H_MAGUI_
#include "app.h"
#include "magdata.h"
#include "config.h"
#include "debug.h"
#include <cybergraphx/cybergraphics.h>
#include <proto/cybergraphics.h>
#include <stdio.h>
#include <exec/types.h>
#include "gfx.h"

#define MAG_MAX_TICK_CALLBACK 10

enum enWindowState {winDefault, winPublic, winNewScreen};

struct MagUIData ;
struct MagControls;

typedef void (*magTickCallback) (struct MagUIData *uidata);

struct MagControls{
	struct MagControls *next;
	struct AppGadget g;
	struct MagParameter *param;
	struct MagUIData *uidata;
};

struct MagUIData
{
	struct Library *CyberGfxBase ;
	FILE *fMag;
	struct IFFMagazineData data;
	enum enWindowState windowMode;
	BOOL isRTG;
	BOOL projectActive;
	Wnd *appWnd;
	UBYTE maxDepth;
	WORD borderTop;		// Preferred border values (similar to struct Window values with minor adjustments)
	WORD borderLeft;	// Preferred border values (similar to struct Window values with minor adjustments)
	struct MagControls *ctls;
	struct MagPage *currentPage;
	ULONG *originalWBColours;
	struct GfxGelSys gels;
	magTickCallback tick_fn[MAG_MAX_TICK_CALLBACK];
	struct BitMap *dbufBitmaps;
	UBYTE dbufActive;
	struct IFFMod *activeMod;
	UBYTE *modBuffer;
	UWORD modLoopCount;
	BOOL modStopOnExit;
	BOOL modHasInit;
};

BOOL magRegisterTick(struct MagUIData *uidata, magTickCallback fn);
void magUnregisterTick(struct MagUIData *uidata, magTickCallback fn);
void magUnregisterAllTicks(struct MagUIData *uidata);

#endif