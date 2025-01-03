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

#define MAGMAXSPRITES 10

static void _effectsSetupSnow(struct MagUIData *uidata);

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