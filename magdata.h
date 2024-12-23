#ifndef __H_MAGDATA
#define __H_MAGDATA

#include "iff.h"
#include <exec/types.h>
#include "config.h"
#include "maggfx.h"

struct IFFmaggfx;

struct MagPage
{
	UWORD imageCount;
	struct MagPage *next;
	struct MagPage *prev;
	struct MagConfig config;
	struct IFFmaggfx *images;
	struct IFFChunkData cmap; // Colour map for all images on page
	ULONG *colourTable ; // a 32bit colour table of the cmap
	UBYTE greyIntensities[256]; // colour to grey mapping
	UWORD lastGadgetID ;
	struct MagColour bg;
	struct MagColour pen;
	struct MagColour shadow;
	struct MagColour highlight;
	
};

struct IFFMagazineData
{
	struct IFFctx ctx;		// Entire IFF image
	UWORD pageCount;
	struct MagPage *pages;
};

struct IFFmaggfx
{
	struct MagPage *page;
	struct IFFmaggfx *next;
	char szName[MAG_MAX_PARAMETER_VALUE];
	struct _BitmapHeader bitmaphdr;
	ULONG camg_flags;
	struct IFFChunkData bmhd;
	struct IFFChunkData cmap;
	struct IFFChunkData body;
	struct IFFChunkData camg;
};

// Setup magazine data (just sets data to zero)
void initMagData(struct IFFMagazineData *iff);
// Free allocated memory
void cleanUpMagData(struct IFFMagazineData *iff);

// Parse IFF file (no memory buffer option, just files)
UWORD parseIFFMagazine(struct IFFMagazineData *iff, FILE *f);

// Find a page using config data. Returns NULL if not found.
// If szName is NULL then first page returned (or NULL if no pages)
struct MagPage *findPage(struct IFFMagazineData *iff, char *szName);

// Find image associated with config value
struct IFFmaggfx *findImage(struct MagPage *page, struct MagValue *imgName);

#endif