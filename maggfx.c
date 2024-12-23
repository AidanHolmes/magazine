#include "maggfx.h"
#include "iff.h"
#include "debug.h"
#include <proto/exec.h>
#include <exec/exec.h>
#include <string.h>

UWORD setClosePens(struct MagColour cText, struct MagColour cBackground, struct MagColour cEdges, struct IFFChunkData *cmap)
{
	return 0;
}

static UBYTE _qspartition(struct ColHistogram *h, UBYTE lo, UBYTE hi, BOOL ascend)
{
	UWORD pivot;
	UBYTE i, j, temp;
	
	pivot = h->byindex[h->byfreq[lo]];
	D(printf("partition: piviot %u, hi %u, lo %u\n", pivot, hi, lo));
	while(1){
		if (ascend){
			for (i=lo; h->byindex[h->byfreq[i]] < pivot && i != hi; i++);
			for (j=hi; h->byindex[h->byfreq[j]] > pivot && j != lo; j--);
			if (i >= j){
				return j;
			}
		}else{
			for (i=lo; h->byindex[h->byfreq[i]] > pivot && i != hi; i++);
			for (j=hi; h->byindex[h->byfreq[j]] < pivot && j != lo; j--);
			if (j >= i){
				return i;
			}
		}
		D(printf("swap %u(%u) with %u(%u)\n", h->byfreq[i], i, h->byfreq[j], j));
		temp = h->byfreq[i];
		h->byfreq[i]=h->byfreq[j];
		h->byfreq[j]=temp;
	}
	return 0;
}

static void _quicksort(struct ColHistogram *h, UBYTE lo, UBYTE hi, BOOL ascend)
{
	UWORD p;
	if (lo >= 0 && hi >= 0 && lo < hi){
		p = _qspartition(h, lo, hi, ascend);
		_quicksort(h, lo, p, ascend);
		_quicksort(h,p+1,hi, ascend);
	}
}

static void _quicksort2(struct ColHistogram *h, UBYTE lo, UBYTE hi, BOOL ascend)
{
	UWORD p =0;
	UBYTE i, j, temp;
	
    while (lo < hi){
        p = h->byindex[h->byfreq[hi]];
        i = lo;
        for (j = lo; j < hi; ++j){
            if (h->byindex[h->byfreq[j]] < p){
				temp = h->byfreq[i];
				h->byfreq[i]=h->byfreq[j];
				h->byfreq[j]=temp;
                ++i;
            }
        }
		temp = h->byfreq[i];
		h->byfreq[i]=h->byfreq[hi];
		h->byfreq[hi]=temp;
        if(i - lo <= hi - i){
            _quicksort2(h, lo, i-1,ascend);
            lo = i+1;
        } else {
            _quicksort2(h, i+1, hi,ascend);
            hi = i-1;
        }
	}
}
#define MAX_LEVELS 256
static void _swap3(struct ColHistogram *h, UBYTE a, UBYTE b)
{
	UBYTE s;
	s = h->byfreq[a];
	h->byfreq[a] = h->byfreq[b];
	h->byfreq[b] = s;
}

static int _quicksort3(struct ColHistogram *h, UWORD elements) 
{
    UBYTE beg[MAX_LEVELS], end[MAX_LEVELS], L, R, M;
    int i = 0, piv = 0;

    beg[0] = 0;
    end[0] = elements;
    while (i >= 0) {
        L = beg[i];
        R = end[i];
        if (R - L > 1) {
            M = L + ((R - L) >> 1);
            piv = h->byindex[h->byfreq[M]];
			h->byfreq[M] = h->byfreq[L] ;

            if (i == MAX_LEVELS - 1)
                return -1;
            R--;
            while (L < R) {
                while (h->byindex[h->byfreq[R]] >= piv && L < R){
                    R--;
				}
                if (L < R){
					_swap3(h,R,L++);
                    //h->byfreq[L++] = h->byfreq[R];
				}
                while (h->byindex[h->byfreq[L]] <= piv && L < R){
                    L++;
				}
                if (L < R){
					_swap3(h,R--,L);
                    //h->byfreq[R--] = h->byfreq[L];
				}
            }
            h->byfreq[L] = piv;
            M = L + 1;
            while (L > beg[i] && h->byfreq[L - 1] == piv)
                L--;
            while (M < end[i] && h->byfreq[M] == piv)
                M++;
            if (L - beg[i] > end[i] - M) {
                beg[i + 1] = M;
                end[i + 1] = end[i];
                end[i++] = L;
            } else {
                beg[i + 1] = beg[i];
                end[i + 1] = L;
                beg[i++] = M;
            }
        } else {
            i--;
        }
    }
    return 0;
}

static void _bubblesort(struct ColHistogram *h, UWORD elements)
{
	UWORD i, j, largest = 0, t;
	for (i=0; i < elements-1; i++){
		largest = i;
		for(j=i+1;j < elements; j++){
			largest = (h->byindex[h->byfreq[largest]] < h->byindex[h->byfreq[j]])? j:largest;
		}
		if (largest != i){
			t = h->byfreq[i] ;
			h->byfreq[i] = h->byfreq[largest];
			h->byfreq[largest] = t;
		}
	}
}

struct ColorSpec* reduceGreyDepth(UWORD Width, UWORD Height, struct BitMap *bmp, struct ColorSpec *cs, UBYTE targetdepth)
{
	UBYTE colours, cidx, newcidx ;
	UWORD i, h, w,d;
	struct ColorSpec *new = NULL;
	ULONG roff;
	UBYTE intensities[255], scale;
	
	colours = (1 << targetdepth) - 1;
	if (colours <=4){
		return FALSE;
	}
	colours -= 4 ; // reserve these for standard colours
	
	scale = 255 / colours;
	
	new =(struct ColorSpec*)AllocVec(sizeof(struct ColorSpec)*colours, MEMF_ANY | MEMF_CLEAR);
	if (!new){
		return NULL;
	}
	D(printf("AllocVec: %s %s(line %s), %u bytes, %p\n", __FUNC__, __FILE__, __LINE__, sizeof(struct ColorSpec)*colours, new));
	
	// Create grey scale colour map
	for(i=0;i<colours;i++){
		new[i+4].Red = scale *i;
		new[i+4].Green = scale *i;
		new[i+4].Blue = scale *i;
		new[i+4].ColorIndex = i+4;
	}
	new[i+4].ColorIndex = -1;
	
	memset(intensities, 0, sizeof(intensities));
	
	// Calculate intensities from existing palette
	for(i=0;cs[i].ColorIndex != -1;i++){
		intensities[cs[i].ColorIndex] = (cs[i].Red + cs[i].Green + cs[i].Blue) / 3 ;	
	}
	
	for (h=0; h < Height; h++){
		roff = h * bmp->BytesPerRow;
		for (w=0; w < Width; w++){
			cidx = 0;
			for (d=0; d < bmp->Depth; d++){
				if (bmp->Planes[d][roff + (w/8)] & (1 << (7 - (w%8)))){
					cidx |= 1 << d;
				}					
			}
			newcidx = (intensities[cidx] / scale) + 4; // add 4 to skip reserved colours
			for (d=0; d < bmp->Depth; d++){
				if (bmp->Planes[d][roff + (w/8)] & (1 << (7 - (w%8)))){
					cidx |= 1 << d;
				}	
				if (newcidx & 1 << d){
					bmp->Planes[d][roff + (w/8)] |= (1 << (7 - (w%8)));
				}else{
					bmp->Planes[d][roff + (w/8)] &= ~(1 << (7 - (w%8)));
				}
			}
		}
	}
	return new;
}

ULONG* createGreyColourTable(ULONG *colourTable, UBYTE intensities[256], UBYTE targetdepth)
{
	WORD colours, i, sourceColours ;
	ULONG *new = NULL, *p = NULL;
	UBYTE scale;
	
	colours = (1 << targetdepth);
	if (colours <=4){
		return NULL;
	}
	if (colours > 256){
		colours = 256;
	}
	
	new =(ULONG*)AllocVec(sizeof(ULONG) * ((colours*3)+2), MEMF_ANY);
	if (!new){
		return NULL;
	}
	new[0] = colours << 16;
	p = new + (4*3)+1; // increment pointer to first colours used for image - skip 4 reserved and size
	
	colours -= 4 ; // reserve these for standard colours
	scale = (256 / colours) + (256%colours?1:0);
	
	// Create grey scale colour map
	for(i=0;i<colours;i++){
		*p++ = ((scale * i) << 24) | 0xFFFFFF;
		*p++ = ((scale * i) << 24) | 0xFFFFFF;
		*p++ = ((scale * i) << 24) | 0xFFFFFF;
	}
	*p++ = 0; // terminator zero value
	
	memset(intensities, 0, 256);
	
	sourceColours = colourTable[0] >> 16;
	if (sourceColours > 256){
		sourceColours = 256; // This function is for anything planar up to 8 bits
	}
	// Calculate intensities from existing palette
	for(i=0,p=colourTable+1;i < sourceColours;i++){
		intensities[i] = ((*p++ >> 24) + (*p++ >> 24) + (*p++ >> 24)) / 3;
	}
	
	return new;
}

BOOL reduceGreyDepthColourTable(UWORD Width, UWORD Height, struct BitMap *bmp, UBYTE intensityMapping[256], UBYTE targetdepth)
{
	UBYTE cidx, newcidx ;
	UWORD h, w,d, colours;
	ULONG roff;
	UBYTE scale;
	
	colours = (1 << targetdepth);
	if (colours <=4){
		return FALSE;
	}
	if (colours > 256){
		colours = 256;
	}
	
	colours -= 4 ; // reserve these for standard colours
	scale = (256 / colours) + (256%colours?1:0);
	
	for (h=0; h < Height; h++){
		roff = h * bmp->BytesPerRow;
		for (w=0; w < Width; w++){
			cidx = 0;
			for (d=0; d < bmp->Depth; d++){
				if (bmp->Planes[d][roff + (w/8)] & (1 << (7 - (w%8)))){
					cidx |= 1 << d;
				}					
			}
			newcidx = (intensityMapping[cidx] / scale) + 4; // add 4 to skip reserved colours
			for (d=0; d < bmp->Depth; d++){	
				if (newcidx & (1 << d)){
					bmp->Planes[d][roff + (w/8)] |= (1 << (7 - (w%8)));
				}else{
					bmp->Planes[d][roff + (w/8)] &= ~(1 << (7 - (w%8)));
				}
			}
		}
	}
	return TRUE;
}

struct ColorSpec* reduceColourDepth(struct ColHistogram *hist, UWORD Width, UWORD Height, struct BitMap *bmp, struct ColorSpec *cs, UBYTE targetdepth)
{
	UBYTE colours, cidx, newcidx ;
	UWORD j, i, h, w,d;
	struct ColorSpec *new = NULL;
	UBYTE remap[255];
	ULONG roff;
	
	memset(remap,0,sizeof(remap));
	
	// TO DO: need to create a new colorspec because all the indexes need remapping to be below the depth required
	// for instance, cannot add palette entry 233 to a 4 depth colour palette
	// The bitmap will need remapping to new colorspec.
	
	colours = (1 << targetdepth) - 1;
	
	new =(struct ColorSpec*)AllocVec(sizeof(struct ColorSpec)*colours, MEMF_ANY | MEMF_CLEAR);
	if (!new){
		return NULL;
	}
	for(j=0;j<colours;j++){
		for(i=0;cs[i].ColorIndex != -1;i++){
			if (hist->byfreq[j] == cs[i].ColorIndex){
				// 
				remap[cs[i].ColorIndex] = j;
				new[j] = cs[i];
				new[j].ColorIndex = j;
			}
		}
	}
	new[j].ColorIndex = -1;
	
	for (h=0; h < Height; h++){
		roff = h * bmp->BytesPerRow;
		for (w=0; w < Width; w++){
			cidx = 0;
			for (d=0; d < bmp->Depth; d++){
				if (bmp->Planes[d][roff + (w/8)] & (1 << (7 - (w%8)))){
					cidx |= 1 << d;
				}					
			}
			newcidx = remap[cidx];
			for (d=0; d < bmp->Depth; d++){
				if (bmp->Planes[d][roff + (w/8)] & (1 << (7 - (w%8)))){
					cidx |= 1 << d;
				}	
				if (newcidx & 1 << d){
					bmp->Planes[d][roff + (w/8)] |= (1 << (7 - (w%8)));
				}else{
					bmp->Planes[d][roff + (w/8)] &= ~(1 << (7 - (w%8)));
				}
			}
		}
	}
	return new;
}


struct ColHistogram* colourHistogram(struct BitMap *bmp, UWORD Width, UWORD Height)
{
	struct ColHistogram *hist;
	UBYTE colours = 0;
	UBYTE i=0;
	UWORD w, h, d, cidx;
	ULONG roff = 0;
	
	if (!(hist=AllocVec(sizeof(struct ColHistogram), MEMF_ANY | MEMF_CLEAR))){
		return NULL;
	}
	
	for (h=0; h < Height; h++){
		roff = h * bmp->BytesPerRow;
		for (w=0; w < Width; w++){
			cidx = 0;
			for (d=0; d < bmp->Depth; d++){
				if (bmp->Planes[d][roff + (w/8)] & (1 << (7 - (w%8)))){
					cidx |= 1 << d;
				}					
			}
			hist->byindex[cidx]++;
		}
	}
	
	colours = (1 << bmp->Depth) -1;
	// Create ref indexes for byindex array
	for(i=0;i<colours;i++){
		hist->byfreq[i]=i;
	}
	_bubblesort(hist, colours);
	//_quicksort3(hist, colours);
	//_quicksort2(hist, 0, colours-1, FALSE);
	
	return hist;
}

void freeHistogram(struct ColHistogram* h)
{
	FreeVec(h);
}

ULONG *copyColourTable(ULONG *colourTable)
{
	ULONG size = 0, *copy = NULL;

	size = ((colourTable[0] >> 16)*3) + 1;
	
	copy = AllocVec(size, MEMF_ANY);
	if (copy){
		memcpy(copy, colourTable, size);
	}
	return copy;
}

ULONG *viewPortColourTable(struct ViewPort *vp)
{
	ULONG *table = NULL ;
	ULONG allocsize = sizeof(ULONG) * ((vp->ColorMap->Count*3)+2);

	table = AllocVec(allocsize, MEMF_ANY);
	if (table){
		GetRGB32(vp->ColorMap, 0, vp->ColorMap->Count, table+1); // copy to offset of 1
		table[0] = vp->ColorMap->Count << 16;
		table[(vp->ColorMap->Count*3)+1] = 0; // Add terminator
	}
	return table;
}

