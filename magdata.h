#ifndef __H_MAGDATA
#define __H_MAGDATA

#include "iff.h"
#include <exec/types.h>
#include "config.h"
#include "maggfx.h"

#define MAX_SCROLL_TEXT_BUFFERS 3
enum enScrollState {SCROLL_TEXT_INIT=1};

struct IFFmaggfx;
struct MagScrollText;
struct MagFormattedText;

struct MagPage
{
	UWORD imageCount;
	struct MagPage *next;
	struct MagPage *prev;
	struct MagConfig config;
	struct IFFmaggfx *images;
	struct MagScrollText *scrollText;
	struct IFFChunkData cmap; // Colour map for all images on page
	ULONG *colourTable ; // a 32bit colour table of the cmap
	UBYTE greyIntensities[256]; // colour to grey mapping
	UWORD lastGadgetID ;
	struct MagColour bg;
	struct MagColour pen;
	struct MagColour shadow;
	struct MagColour highlight;
	
};
	
struct MagFormattedText{
	struct MagPage *page;
	struct MagFormattedText *next;
	// Public attributes
	struct TextAttr font;   // Preferred font
	char *txt; 				// The text string to use in scroller
	UWORD length; 			// Length of the text string
	UWORD x; 				// X Position on the screen
	UWORD y;				// Y Position on the screen
	UWORD height; 			// Height of the scroller. Should be at least height of text
	UWORD width;			// Width of the scroller
	UWORD pen;				// Index of pen to use for text
	
	// Private attributes
	struct TextFont textFont;
};

struct MagScrollText
{
	struct MagFormattedText fmtTxt;
	// Public attributes
	UWORD speed;			// Speed in pixels for each 'tick' cycle
	
	// Private attributes
	UWORD charoffset ; // scroll offset in txt buffer
	UWORD flags;
	struct BitMap *backgnd; // original image background for blits
	struct RastPort textRastPort;
	UWORD actual_width[MAX_SCROLL_TEXT_BUFFERS];
	WORD xoff[MAX_SCROLL_TEXT_BUFFERS]; // x scroll offset for textPort
	BOOL active[MAX_SCROLL_TEXT_BUFFERS];
};

struct IFFMod;

struct IFFMod
{
	struct IFFMod *next;
	char szName[MAG_MAX_PARAMETER_VALUE];
	struct IFFChunkData sequenceData;
};

struct IFFMagazineData
{
	struct IFFctx ctx;		// Entire IFF image
	UWORD pageCount;
	UWORD modCount;
	struct IFFMod *mods;
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

// Find a music mod using the config name value
struct IFFMod *findMod(struct IFFMagazineData *iff, struct MagValue *modName);

// Add scroll text data to the current page. Allocation of memory is private to function and return pointer should not 
// be freed. removeAllScrollText will remove allocated memory on a page. 
//struct MagScrollText *addScrollText(struct MagUIData *uidata, char *txt, UWORD len, UWORD x, UWORD y, UWORD w, UWORD h, UWORD speed, UWORD pen);
struct MagScrollText *addScrollText(struct MagUIData *uidata, struct MagScrollText *scrl);

// Free scroll text memory from a page.
void removeAllScrollText(struct IFFMagazineData *iff, struct MagPage *page);

#endif