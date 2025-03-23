#include <string.h>
#include <proto/intuition.h>
#include <intuition/intuition.h>
#include <proto/exec.h>
#include <exec/exec.h>
#include <clib/icon_protos.h>
#include <clib/dos_protos.h>
#include "magdata.h"
#include "maggfx.h"
#include "magui.h"
#include "magpages.h"
#include "mageffects.h"

#define APP_WIDTH 640
#define APP_HEIGHT 512

const char __ver[] = "$VER: Mag Reader 1.2.1 (22.03.2025)";

static struct TextAttr _topaz8 = {
   (STRPTR)"topaz.font", 8, 0, 1
};

BOOL loadParameters(struct MagUIData *uidata, int argc, char **argv)
{
	struct WBStartup *WBenchMsg = NULL;
	struct WBArg *wbarg = NULL;
	struct DiskObject *dobj = NULL;
	LONG olddir = -1;
	struct Library *IconBase = NULL;
	char *strToolValue = NULL;
	
	// Args from command line 
	if (argc > 0){
		if (argc >= 2){
			uidata->fMag = fopen(argv[1], "rb");
		}
		if (argc >= 3){
			if (magstricmp(argv[2], "WINDOW", 10)){
				uidata->windowMode = winPublic;
			}else if (magstricmp(argv[2], "SCREEN",10)){
				uidata->windowMode = winNewScreen;
			}
		}
	}else{
		// Started from Workbench
		if(!(IconBase = OpenLibrary("icon.library",33))){
			return FALSE;
		}
		WBenchMsg = (struct WBStartup *)argv;
		wbarg=WBenchMsg->sm_ArgList;

		if (WBenchMsg->sm_NumArgs >= 2){
			if (wbarg[1].wa_Lock && wbarg[1].wa_Name){
				olddir = CurrentDir(wbarg[1].wa_Lock);
				
				uidata->fMag = fopen(wbarg[1].wa_Name, "rb");
				
				if((dobj=GetDiskObject(wbarg[1].wa_Name))){
					if ((strToolValue = (char *)FindToolType((char **)dobj->do_ToolTypes, "MODE"))){
						if (magstricmp(strToolValue, "WINDOW",10)){
							uidata->windowMode = winPublic;
						}else if (magstricmp(strToolValue, "SCREEN",10)){
							uidata->windowMode = winNewScreen;
						}
					}
				}
				
				if (dobj){
					FreeDiskObject(dobj);
					dobj = NULL;
				}

				if (olddir == -1){
					CurrentDir(olddir);
				}
			}
		}

		CloseLibrary(IconBase);
	}
	
	if (!uidata->fMag){
		return FALSE;
	}
	return TRUE ;
}



void closeProject(struct MagUIData *uidata)
{
	if (uidata->currentPage){
		uiClearPage(uidata);
		uidata->currentPage = NULL;
	}
	if (uidata->fMag){
		fclose(uidata->fMag);
		uidata->fMag = NULL;
		cleanUpMagData(&uidata->data);
	}
	if (uidata->originalWBColours){
		freeColourTable(uidata->originalWBColours);
		uidata->originalWBColours = NULL;
	}
}

BOOL loadProject(struct MagUIData *uidata)
{
	// This function requires an open file in uidata - this is the responsibilty of the calling function
	uidata->projectActive = FALSE;
	initMagData(&uidata->data);
	if (parseIFFMagazine(&uidata->data, uidata->fMag) == IFF_NO_ERROR){
		
		if (!uiOpenPage(uidata, NULL)){ // open first page
			closeProject(uidata);
		}
		uidata->projectActive = TRUE;
	}
	
	return uidata->projectActive;
}

void window_sigs (struct App *myApp, ULONG sigs)
{
	if (sigs == SIGBREAKF_CTRL_C){
		myApp->closedispatch = TRUE ;
	}
}

void window_event(Wnd *myWnd, ULONG idcmp)
{
	struct MagUIData *uidata = myWnd->app->appContext ;
	
	if (uidata->windowMode == winNewScreen){
		return;
	}

	if (idcmp == IDCMP_INACTIVEWINDOW){
		if (uidata->originalWBColours){
			setViewPortColorTable(&uidata->data.ctx, &myWnd->app->appScreen->ViewPort, uidata->originalWBColours, uidata->maxDepth);
		}
	}else if (idcmp == IDCMP_ACTIVEWINDOW){
		if (uidata->currentPage && uidata->currentPage->colourTable){
			setViewPortColorTable(&uidata->data.ctx, &myWnd->app->appScreen->ViewPort, uidata->currentPage->colourTable, uidata->maxDepth);
		}
	}
}

void window_tick(struct Wnd *myWnd)
{
	UBYTE i =0;
	struct MagUIData *uidata = myWnd->app->appContext ;
	
	for(;i<MAG_MAX_TICK_CALLBACK;i++){
		if (uidata->tick_fn[i]){
			uidata->tick_fn[i](uidata);
		}
	}
}

void freeDoubleBuffer(struct MagUIData *uidata)
{
	UBYTE i=0,b=0;
	
	if (uidata->dbufBitmaps){
		for (b=0;b < 2;b++){
			for (i=0;i<uidata->maxDepth;i++){
				if (uidata->dbufBitmaps[b].Planes[i]){
					FreeRaster(uidata->dbufBitmaps[b].Planes[i], APP_WIDTH, APP_HEIGHT);
				}
			}
		}
		FreeVec(uidata->dbufBitmaps);
		uidata->dbufBitmaps = NULL ;
	}
}

BOOL initDoubleBuffer(struct MagUIData *uidata)
{
	UBYTE i = 0, b=0;
	BOOL ret = FALSE ;
	
	if (!(uidata->dbufBitmaps = (struct BitMap *)AllocVec(sizeof(struct BitMap)*2, MEMF_CLEAR))){
		return FALSE ;
	}
	
	for (b=0;b < 2;b++){
		InitBitMap(&uidata->dbufBitmaps[b], uidata->maxDepth, APP_WIDTH, APP_HEIGHT);
		for (i=0;i<uidata->maxDepth;i++){
			if (uidata->dbufBitmaps[b].Planes[i] = (PLANEPTR)AllocRaster(APP_WIDTH,APP_HEIGHT)){
				BltClear((APTR)uidata->dbufBitmaps[b].Planes[i], (APP_WIDTH/8)*APP_HEIGHT, 1);
			}else{
				goto cleanup;
			}
		}
	}
	uidata->appWnd->app->screeninfo.Type |= CUSTOMBITMAP;
	uidata->appWnd->app->screeninfo.CustomBitMap = uidata->dbufBitmaps;
	
	ret = TRUE ;
cleanup:
	if (!ret){
		freeDoubleBuffer(uidata);
	}
	return ret;
}

int main(int argc, char **argv)
{
	App myApp ;
	UWORD pens[1] = {(UWORD)~0};
	ULONG winerror = 0;
	ULONG screenTags[] = {SA_Colors,0,SA_Depth,4,SA_Pens,0,SA_Font,0,SA_ErrorCode,0,TAG_END};
	ULONG screenTagsRTG[] = {SA_DisplayID,0,SA_Pens,0,SA_Font,0,TAG_END};
	ULONG wndTags[] = {WA_AutoAdjust,TRUE,TAG_END};
	ULONG displayTagsRTG[] = {CYBRBIDTG_Depth,24,CYBRBIDTG_NominalWidth,APP_WIDTH,CYBRBIDTG_NominalHeight,APP_HEIGHT};
	ULONG *prefScreenTags = NULL ;
	struct Library *CyberGfxBase = NULL ;
	struct MagUIData uidata;
	BOOL emptyProject = FALSE ;
	
	screenTags[5] = (ULONG)pens;
	screenTags[7] = (ULONG)&_topaz8;
	screenTags[9] = (ULONG)&winerror;
	screenTagsRTG[3] = (ULONG)pens;
	screenTagsRTG[5] = (ULONG)&_topaz8;
	
	//SetTaskPri(FindTask(NULL), 100);
	
	memset(&uidata, 0, sizeof(struct MagUIData));
	
	emptyProject = !loadParameters(&uidata, argc, argv);
	
	initialiseApp(&myApp);
	
	myApp.appContext = &uidata;
	myApp.wake_sigs = SIGBREAKF_CTRL_C;
	myApp.fn_wakeSigs = window_sigs;
	myApp.fn_intuiTicks = window_tick;

	// Run detection of RTG
	if (myApp.isScreenRTG){
		// Also check for cybergraphics support and the screen type
		CyberGfxBase = uidata.CyberGfxBase=OpenLibrary("cybergraphics.library",41L);
		if (CyberGfxBase){
			if(GetCyberMapAttr(myApp.appScreen->RastPort.BitMap, CYBRMATTR_ISCYBERGFX)){
				uidata.isRTG = TRUE;
			}
		}
	}
	
	if (uidata.windowMode == winDefault){
		uidata.windowMode = winNewScreen;
		// Determine best mode
		if (uidata.isRTG){
			uidata.windowMode = winPublic;
		}
	}

	uidata.appWnd = getAppWnd(&myApp);
	uidata.maxDepth = 4 ;
	uidata.appWnd->fn_windowEvent = window_event;
	
	if(uidata.windowMode == winNewScreen){
		prefScreenTags = screenTags;
		if (uidata.isRTG){
			screenTagsRTG[1] = BestCModeIDTagList((struct TagItem*)displayTagsRTG);
			prefScreenTags = screenTagsRTG;
		}else{
			if (gfxChipSet() == GFX_IS_AGA){
				// set depth to 8 bits
				uidata.maxDepth = 8;
				prefScreenTags[3] = (ULONG)uidata.maxDepth;
			}
		}
		// RTG really doesn't like double buffering so exclude this setup if RTG
		if (!uidata.isRTG){
			initDoubleBuffer(&uidata);
		}
		if (createAppScreen(&myApp, TRUE, TRUE, prefScreenTags) != 0){
			printf("Cannot open screen, error %d\n", winerror);
			goto exitcleanup;
		}
		wndSetSize(uidata.appWnd, myApp.appScreen->Width, myApp.appScreen->Height);
		uidata.appWnd->info.Flags = WFLG_BORDERLESS | WFLG_ACTIVATE ;
	}else{
		// Standard public workbench screen
		wndSetSize(uidata.appWnd, APP_WIDTH, APP_HEIGHT);
		uidata.appWnd->info.Flags = WFLG_DRAGBAR | WFLG_CLOSEGADGET | WFLG_GIMMEZEROZERO ;
		// Check depth and available screen size
		
		if (getScreenWidth(&myApp) < APP_WIDTH ||
			getScreenHeight(&myApp) < APP_HEIGHT){
			printf("Cannot display on public screen without a resolution of at least 640x512\n");
			goto exitcleanup;
		}
		
		uidata.originalWBColours = viewPortColourTable(&myApp.appScreen->ViewPort);
		if (!uidata.originalWBColours){
			goto exitcleanup;
		}
	}
	uidata.appWnd->idcmp |= IDCMP_INTUITICKS;
	
	// Can only check screen attributes prior to window opening, otherwise screen pointer has been freed and null
	uidata.maxDepth = getScreenDepth(&myApp);

	if (openAppWindow(uidata.appWnd, wndTags) != 0){
		goto exitcleanup;
	}
	
	// Double buffer setup
	if (uidata.dbufBitmaps){
		uidata.appWnd->appWindow->WScreen->RastPort.Flags = DBUFFER;
		uidata.appWnd->appWindow->WScreen->RastPort.BitMap = &uidata.dbufBitmaps[0];
	}
	
	uidata.borderTop = uidata.borderLeft = 0; // don't have any predefined border, let config decide
	
	if (!initialiseGelSys(uidata.appWnd)){
		goto exitcleanup;
	}
	
	if (!initModMusic(&uidata)){
		goto exitcleanup;
	}
	
	if (!emptyProject){
		loadProject(&uidata);
	}
	
	// Enter UI dispatch loop
	dispatch(&myApp);
	
exitcleanup:
	// App closing - clean up from here and exit
	cleanupModMusic(&uidata);
	if (uidata.originalWBColours){
		setViewPortColorTable(&uidata.data.ctx, &myApp.appScreen->ViewPort, uidata.originalWBColours, uidata.maxDepth);
		RemakeDisplay();
	}
	closeProject(&uidata);
	cleanupGelSys(uidata.appWnd);
	freeDoubleBuffer(&uidata);
    appCleanUp(&myApp);
	
	if (CyberGfxBase){
		CloseLibrary(CyberGfxBase);
	}
	
	return 0;
}

BOOL magRegisterTick(struct MagUIData *uidata, magTickCallback fn)
{
	UBYTE i = 0;
	for (;i<MAG_MAX_TICK_CALLBACK;i++){
		if (uidata->tick_fn[i] == NULL){
			uidata->tick_fn[i] = fn;
			return TRUE;
		}
	}
	
	return FALSE ;
}

void magUnregisterTick(struct MagUIData *uidata, magTickCallback fn)
{
	UBYTE i = 0;
	for (;i<MAG_MAX_TICK_CALLBACK;i++){
		if (uidata->tick_fn[i] == fn){
			uidata->tick_fn[i] = NULL;
			break;
		}
	}
}

void magUnregisterAllTicks(struct MagUIData *uidata)
{
	UBYTE i = 0;
	for (;i<MAG_MAX_TICK_CALLBACK;i++){
		uidata->tick_fn[i] = NULL;
	}
}