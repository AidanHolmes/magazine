#ifndef __MAGGRAPHICS_HDR
#define __MAGGRAPHICS_HDR

#include <exec/types.h>

struct MagColour{
	UBYTE r;
	UBYTE g;
	UBYTE b;
};

struct ColHistogram{
	UWORD byindex[255];
	UBYTE byfreq[255];
};

// Get a colour frequency histogram 
struct ColHistogram* colourHistogram(struct BitMap *bmp, UWORD Width, UWORD Height);
void freeHistogram(struct ColHistogram* h);

////////////////////////////////////////////////////////////
// Colour Spec Functions
// - Will not use these going forward unless better for RTG
// - Alternative colour table functions available
//

// Directly reorganises the colorspec to most frequent colour first, up to depth number of colours.
struct ColorSpec* reduceColourDepth(struct ColHistogram *hist, UWORD Width, UWORD Height, struct BitMap *bmp, struct ColorSpec *cs, UBYTE targetdepth);
struct ColorSpec* reduceGreyDepth(UWORD Width, UWORD Height, struct BitMap *bmp, struct ColorSpec *cs, UBYTE targetdepth);


///////////////////////////////////////////////////////
// Colour Table Functions
// - Faster functions can be used in Gfx library
//

// Create a colour table compatible with LoadRGB32 from a ViewPort struct. Use IFF functionality to free.
ULONG *viewPortColourTable(struct ViewPort *vp);

// Return a grey scale colour table from colour version provided in parameters.
// Creates values in intentities table for mapping of colour indexes to grey.
// Grey scale colour table must be freed with freeColourTable from IFF library
ULONG* createGreyColourTable(ULONG *colourTable, UBYTE intensities[256], UBYTE targetdepth);

// Updates provided bitmap with mapping to grey colour table generated from createGreyColourTable. Must use same targetdepth and intensity mapping
// create by createGreyColourTable.
BOOL reduceGreyDepthColourTable(UWORD Width, UWORD Height, struct BitMap *bmp, UBYTE intensityMapping[256], UBYTE targetdepth);

// Create copy (for restoring and other things). use IFF functionality to free (or FreeVec)
ULONG *copyColourTable(ULONG *colourTable);
















#endif