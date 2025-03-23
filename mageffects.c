#include "gfx.h"
#include "mageffects.h"
#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "snowflake1.h"
#include "snowflake2.h"
#include "snow1.h"
#include <proto/timer.h>
#include <devices/timer.h>
#include <exec/memory.h>
#include <proto/graphics.h>

#define MAGMAXSPRITES 10

static void _effectsSetupSnow(struct MagUIData *uidata);
static void _effectsSetupTextScroll(struct MagUIData *uidata);

static struct MagSprite{
	struct VSprite sprite;
	ULONG virtualX; // 1/1000 pixel resolution
	ULONG virtualY; // 1/1000 pixel resolution 
	WORD vectorX; // pixels per minute
	WORD vectorY; // pixels per minute
	BOOL visible;
	struct timeval lastActivity;
	struct timeval activeAt;
} magVs[MAGMAXSPRITES];
	

BOOL effectsInit(struct MagUIData *uidata, char *szEffect)
{
	if (magstricmp(szEffect, "Snowfall", MAG_MAX_PARAMETER_NAME)){
		_effectsSetupSnow(uidata);
		return TRUE;
	}else if (magstricmp(szEffect, "TextScroll", MAG_MAX_PARAMETER_NAME)){
		_effectsSetupTextScroll(uidata);
		return TRUE;
	}
	
	// Do not recognise effect
	return FALSE;
}

void stopModMusic(struct MagUIData *uidata)
{
	void *base = (void*)0xdff000;
	if (!uidata){
		return;
	}
	mt_Enable = 0;
	if (uidata->activeMod){
		mt_end(base);
	}
	if (uidata->modBuffer){
		FreeVec(uidata->modBuffer);
		uidata->modBuffer=NULL;
	}
	uidata->activeMod = NULL;
}

BOOL restartModMusic(struct MagUIData *uidata)
{
	void *base = (void*)0xdff000;
	if (!uidata || !uidata->activeMod){
		return FALSE;
	}
	
	mt_init(base, uidata->modBuffer, NULL, 0);
	mt_Enable = 1;
	
	return TRUE;
}

BOOL startModMusic(struct MagUIData *uidata, struct IFFMod *mod)
{
	void *base = (void*)0xdff000;
	
	if (!uidata || !mod){
		return FALSE;
	}
	uidata->activeMod = NULL;
	
	// Seek to start of mod music data
	fseek(uidata->data.ctx.f, mod->sequenceData.offset, SEEK_SET);
	// Allocate memory in chip ram for the music
	uidata->modBuffer = AllocVec(mod->sequenceData.length, MEMF_CHIP);
	if (!uidata->modBuffer){
		return FALSE ;
	}
	if (!fread(uidata->modBuffer, mod->sequenceData.length, 1, uidata->data.ctx.f)){
		return FALSE ;
	}
	
	// set the active mod name as flag and reference of mod playing
	uidata->activeMod = mod;
	
	mt_init(base, uidata->modBuffer, NULL, 0);
	mt_Enable = 1;
	
	return TRUE ;
}

BOOL initModMusic(struct MagUIData *uidata)
{
	if (!uidata){
		return FALSE ;
	}
	if (!mt_install()){
		return FALSE;
	}
	uidata->activeMod = NULL ;
	uidata->modHasInit = TRUE ;
	return TRUE;
}

void cleanupModMusic(struct MagUIData *uidata)
{
	if (uidata){
		stopModMusic(uidata);
		if (uidata->modHasInit){
			mt_remove();
		}
	}
}

__inline LONG magRand(LONG min, LONG max)
{
	LONG range = max - min;
	
	return (rand()/(RAND_MAX/range)) + min;
}

void musicTickEvent(struct MagUIData *uidata)
{
	if (uidata->activeMod){
		// Music is playing
		if (mt_SongEnd){ // Music playing has ended
			if (uidata->modLoopCount == 1){
				// This was the last play, stop music
				stopModMusic(uidata);
			}else{
				// repeat
				restartModMusic(uidata);
			}
			if (uidata->modLoopCount > 0){
				uidata->modLoopCount--;
			}
		}
	}
}

static void _snowTickEvent(struct MagUIData *uidata)
{
	UWORD i = 0;
	LONG elapsed = 0;
	struct Window *wnd = NULL;
	struct timeval now, diff;
	struct Device *TimerBase = uidata->appWnd->app->tmr->io_Device;

	wnd = uidata->appWnd->appWindow;
	
	GetSysTime(&now);
	
	for (;i<MAGMAXSPRITES;i++){
		if (!magVs[i].visible && magVs[i].activeAt.tv_secs < now.tv_secs){
			if (magRand(0,100) > 60){
				magVs[i].visible = TRUE ;
				magVs[i].sprite.PlanePick = (1 << magVs[i].sprite.Depth) - 1;
				magVs[i].sprite.X = magRand(0,uidata->appWnd->info.Width);
				magVs[i].sprite.Y = 0;
				magVs[i].virtualX = magVs[i].sprite.X * 1000;
				magVs[i].virtualY = 0;
				magVs[i].vectorX = magRand(-240,240);
				magVs[i].vectorY = magRand(240,1500);
				magVs[i].lastActivity = now;
			}else{
				magVs[i].activeAt = now;
				magVs[i].activeAt.tv_secs += magRand(1,5);
			}
		}else if(magVs[i].visible){
			diff = now;
			SubTime(&diff, &magVs[i].lastActivity);
			magVs[i].lastActivity = now;
			elapsed = diff.tv_micro + (diff.tv_secs * 1000000);
			magVs[i].virtualX += (magVs[i].vectorX * elapsed) / 60000;
			magVs[i].virtualY += (magVs[i].vectorY * elapsed) / 60000;
			magVs[i].sprite.X = magVs[i].virtualX / 1000;
			magVs[i].sprite.Y = magVs[i].virtualY / 1000;
			if (magVs[i].sprite.X > uidata->appWnd->info.Width || magVs[i].sprite.X <=0 ||
			    magVs[i].sprite.Y > uidata->appWnd->info.Height){
				magVs[i].visible = FALSE ;
				magVs[i].sprite.PlanePick = 0;
			}					
		}
	}
	SortGList(wnd->RPort);
	if (uidata->dbufBitmaps){
		wnd->WScreen->ViewPort.RasInfo->BitMap = &uidata->dbufBitmaps[uidata->dbufActive];
		uidata->dbufActive ^= 1;
	}
	DrawGList(wnd->RPort, &wnd->WScreen->ViewPort);
	if (uidata->dbufBitmaps){
		MakeScreen(wnd->WScreen);
		RethinkDisplay();
		wnd->RPort->BitMap = &uidata->dbufBitmaps[uidata->dbufActive];
	}else{
		WaitTOF() ;
	}
}
	
static void _effectsSetupSnow(struct MagUIData *uidata)
{
	UWORD i = 0, idx;
	struct timeval now;
	struct Device *TimerBase = uidata->appWnd->app->tmr->io_Device;
	GetSysTime(&now);
	
	srand(now.tv_secs+(now.tv_secs * 1000000));
	for (i=0;i<MAGMAXSPRITES;i++){
		idx = (i % 3);
		memset(&magVs[i],0,sizeof(struct MagSprite));
		magVs[i].sprite.Y = magVs[i].sprite.X = 0;
		magVs[i].sprite.Flags = SAVEBACK | OVERLAY;
		magVs[i].sprite.MeMask = 0;
		magVs[i].sprite.HitMask = 0;
		magVs[i].sprite.SprColors = NULL;
		magVs[i].sprite.PlaneOnOff = 0;
		if (idx == 1){
			magVs[i].sprite.Width = sf1_sprite_word_width;
			magVs[i].sprite.Depth = sf1_sprite_planes;
			magVs[i].sprite.Height = sf1_sprite_height;
			magVs[i].sprite.ImageData = sf1_sprite_data;
		}else if(idx == 2){
			magVs[i].sprite.Width = sn1_sprite_word_width;
			magVs[i].sprite.Depth = sn1_sprite_planes;
			magVs[i].sprite.Height = sn1_sprite_height;
			magVs[i].sprite.ImageData = sn1_sprite_data;
		}else{
			magVs[i].sprite.Width = sf2_sprite_word_width;
			magVs[i].sprite.Depth = sf2_sprite_planes;
			magVs[i].sprite.Height = sf2_sprite_height;
			magVs[i].sprite.ImageData = sf2_sprite_data;
			
		}
		magVs[i].sprite.PlanePick = 0; // hide until needed
		if (!createBob(uidata->appWnd, &uidata->gels, &magVs[i].sprite,uidata->maxDepth,uidata->dbufBitmaps?TRUE:FALSE)){
			removeBobs(uidata->appWnd, &uidata->gels);
			return;
		}
	}
	if (uidata->dbufBitmaps){
		BltBitMap(&uidata->dbufBitmaps[uidata->dbufActive], 0, 0, 
					&uidata->dbufBitmaps[uidata->dbufActive ^ 1], 0,0,640,512,0xC0,0xFF,0);
	}
	magRegisterTick(uidata, _snowTickEvent);
}


static UWORD _fillTextRastPort(struct MagScrollText *txt, UWORD bufferNo)
{
	struct TextExtent te ;
	UWORD charsToFit = 0;
	UWORD txtLen = 0;
	
	// Fix bufferNo to 0, 1 or 2
	if (bufferNo >= MAX_SCROLL_TEXT_BUFFERS){
		bufferNo = MAX_SCROLL_TEXT_BUFFERS-1;
	}
	
	// Move text render to active buffer offset
	Move(&txt->textRastPort, 0, txt->textFont.tf_YSize + (bufferNo * txt->height));
	
	txtLen = txt->length - txt->charoffset;
	if (txtLen == 0){
		txt->charoffset=0;
		txtLen = txt->length;
	}
	charsToFit = TextFit(&txt->textRastPort, txt->txt + txt->charoffset, txtLen, &te, NULL, 1, txt->width, 30000);
	if (charsToFit == 0){
		// no text fits. Fast forward to end of string
		txt->charoffset = txt->length;
		return txt->length;
	}
	txt->actual_width[bufferNo] = te.te_Width;
	Text(&txt->textRastPort, txt->txt + txt->charoffset, charsToFit); // write to text rastport
	
	txt->charoffset += charsToFit;
	return charsToFit;
}

static BOOL _initTextScroll(struct Window *scrWnd, struct MagScrollText *txt)
{
	if((txt->flags & SCROLL_TEXT_INIT) == 0){
		SetAPen(&txt->textRastPort, txt->pen); 
		// First run - fill buffer with text to start print scroll
		_fillTextRastPort(txt,0);
		
		// Reset offset positions with the new active buffer at offset 0
		txt->xoff[0] = txt->width;
		txt->active[0] = TRUE;
		txt->xoff[1] = txt->width;
		txt->active[1] = FALSE;
		txt->xoff[2] = txt->width;
		txt->active[2] = FALSE;
		
		// Copy background into image rastport cache
		BltBitMap(scrWnd->RPort->BitMap, txt->x, txt->y, txt->backgnd, 0,0,txt->width,txt->height,0x0C0,0xFF,NULL);
		
		txt->flags |= SCROLL_TEXT_INIT;
	}
	return TRUE ;
}

static BOOL _drawTextScroll(struct Window *scrWnd, struct MagScrollText *txt)
{
	WORD dx=0,sx=0,w=0,i=0,j=0;
	
	WaitBlit();
	// Blit in the background
	//BltBitMapRastPort(txt->backgnd, 0, 0, scrWnd->RPort, txt->x, txt->y,txt->width,txt->height,0x0C0);

	for(i=0;i<MAX_SCROLL_TEXT_BUFFERS;i++){
		dx=sx=w=0;
		
		if (txt->active[i]){
			// Set destination of bitblt
			if (txt->xoff[i] >= 0){
				dx = txt->x + txt->xoff[i];
				w = txt->width - txt->xoff[i];
				if (w > txt->actual_width[i]){
					w = txt->actual_width[i];
				}
			}else{
				dx = txt->x;
				w = txt->actual_width[i] + txt->xoff[i];
				sx = -(txt->xoff[i]);
			}
			
			BltBitMapRastPort(txt->textRastPort.BitMap, sx, i * txt->height, scrWnd->RPort, dx, txt->y,w,txt->height,0x0C0);
		
			j=i+1;
			if (j>=MAX_SCROLL_TEXT_BUFFERS){j=0;} // Next buffer
			
			if ((w == txt->actual_width[i] || txt->xoff[i] < 0) && txt->active[j] == FALSE){
				
				_fillTextRastPort(txt,j); // add new text
				txt->active[j] = TRUE ;
				if (j == 0){
					txt->xoff[j] -= txt->speed;
				}
			}
			txt->xoff[i] -= txt->speed;
			if (txt->xoff[i] < (-txt->actual_width[i])){ // completed full scroll
				txt->xoff[i] = txt->width; // reset to right side of scroll area
				txt->active[i] = FALSE ;
			}
		}
	}
	
	return TRUE ;
}

static void _textTickEvent(struct MagUIData *uidata)
{
	//struct timeval now;
	struct MagPage *page = NULL ;
	struct MagScrollText *txt = NULL;
	//struct Device *TimerBase = uidata->appWnd->app->tmr->io_Device;
	
	if (!(page=uidata->currentPage)){
		return;
	}
	
	if (!page->scrollText){
		return;
	}
	
	//GetSysTime(&now);
	
	// Iterate through all scroll texts on page
	for(txt=page->scrollText;txt;txt=txt->next){
		if((txt->flags & SCROLL_TEXT_INIT) == 0){
			// Create scroll text
			_initTextScroll(uidata->appWnd->appWindow, txt);
		}else{
			// initialised
			_drawTextScroll(uidata->appWnd->appWindow, txt);
		}
	}
}

static void _effectsSetupTextScroll(struct MagUIData *uidata)
{
	magRegisterTick(uidata, _textTickEvent);
}