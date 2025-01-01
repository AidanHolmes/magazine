#include "magpages.h"
#include "magdata.h"
#include "maggfx.h"
#include "magui.h"
#include "mageffects.h"
#include <stdio.h>
#include <string.h>
#include <exec/types.h>
#include <intuition/gadgetclass.h>
#include <exec/exec.h>
#include <intuition/intuitionbase.h>
#include <proto/graphics.h>

#define CyberGfxBase uidata->CyberGfxBase

static struct TextAttr topaz8 = {
   (STRPTR)"topaz.font", 8, 0, 1
};

static void _btnCmdExit(struct AppGadget *g, struct IntuiMessage *m)
{
	closeAppWindow(g->wnd);
	g->wnd->app->closedispatch = TRUE;	
}

static void _btnNext(struct AppGadget *g, struct IntuiMessage *m)
{
	
}

static void _btnPrev(struct AppGadget *g, struct IntuiMessage *m)
{

}

static void _btnNav(struct AppGadget *g, struct IntuiMessage *m)
{
	struct MagControls *ctl = NULL;
	struct MagValue *val = NULL;
	
	ctl = g->data;
	if ((val=findValue("NAVTO", ctl->param))){
		if (findPage(&ctl->uidata->data, val->szValue)){
			uiOpenPage(ctl->uidata, val->szValue);
		}
	}
}

__inline static void _setPageColour(struct MagUIData *uidata, ULONG *colourTable, UWORD index, UBYTE r, UBYTE g, UBYTE b)
{
	UWORD offsetindex = 0, *colourTable4 = NULL;
	
	if (uidata->appWnd->app->gfx->lib_Version >= 39){
		offsetindex = (index * 3)+1;
		colourTable[offsetindex++] = (r << 24) | 0xFFFFFF;
		colourTable[offsetindex++] = (g << 24) | 0xFFFFFF;
		colourTable[offsetindex++] = (b << 24) | 0xFFFFFF;
	}else{
		offsetindex = index+1;
		colourTable4 = (UWORD*)colourTable;
		colourTable4[offsetindex] = ((r & 0xF0) << 4) | (g & 0xF0) | ((b & 0xF0) >> 4);
	}
}

static void _applyPageColours(struct MagUIData *uidata, ULONG *colourTable)
{
	_setPageColour(uidata, colourTable, 0, uidata->currentPage->bg.r, 
											uidata->currentPage->bg.g, 
											uidata->currentPage->bg.b);
											
	_setPageColour(uidata, colourTable, 1, uidata->currentPage->shadow.r, 
											uidata->currentPage->shadow.g, 
											uidata->currentPage->shadow.b);
											
	_setPageColour(uidata, colourTable, 2, uidata->currentPage->pen.r, 
											uidata->currentPage->pen.g, 
											uidata->currentPage->pen.b);

	_setPageColour(uidata, colourTable, 3, uidata->currentPage->highlight.r, 
											uidata->currentPage->highlight.g, 
											uidata->currentPage->highlight.b);
}

static BOOL _openPageImage(struct MagUIData *uidata, struct MagParameter *param, struct IFFmaggfx *img)
{
	UWORD x=0,y=0,w=0,h=0;
	struct BitMap *bmpimg = NULL;
	struct IFFRenderInfo ri;
	BOOL ret = FALSE ;
	ULONG *colourTableGrey = NULL;
	
	ri.Memory = NULL;
	
	x = magatouw(findValue("X", param), 0);
	y = magatouw(findValue("Y", param), 0);
	w = magatouw(findValue("WIDTH", param), img->bitmaphdr.Width );
	h = magatouw(findValue("HEIGHT", param), img->bitmaphdr.Height);
	
	x += uidata->borderLeft;
	y += uidata->borderTop;
	
	bmpimg = createBitMap(&uidata->data.ctx, &img->body, &img->bitmaphdr, NULL);
	if (!bmpimg){
		goto cleanup;
	}

	if (!uidata->currentPage->colourTable){
		// If no colour table exists for the page then create from first image.
		if (!(uidata->currentPage->colourTable = createColorTable(&uidata->data.ctx,&img->page->cmap,uidata->maxDepth))){
			goto cleanup;
		}
		if (img->bitmaphdr.Bitplanes > uidata->maxDepth){
			if ((colourTableGrey=createGreyColourTable(uidata->currentPage->colourTable, uidata->currentPage->greyIntensities, uidata->maxDepth))){
				freeColourTable(uidata->currentPage->colourTable); // don't need the colour version, so delete
				uidata->currentPage->colourTable = colourTableGrey;
				colourTableGrey = NULL; // not going to use this again as it now replaces the colour version
			}
		}
		_applyPageColours(uidata, uidata->currentPage->colourTable); // add 4 pen colours for other rendering
	}
	
	if (img->bitmaphdr.Bitplanes > uidata->maxDepth){
		if (!reduceGreyDepthColourTable(img->bitmaphdr.Width,img->bitmaphdr.Height,bmpimg, uidata->currentPage->greyIntensities, uidata->maxDepth)){
			goto cleanup;
		}
	}
	
	if (uidata->isRTG){
		if (convertPlanarTableTo24bitRender(&ri, img->bitmaphdr.Width, img->bitmaphdr.Height, bmpimg, uidata->currentPage->colourTable) == IFF_NO_ERROR){
			if (!ri.Memory){
				goto cleanup;
			}
			WritePixelArray(ri.Memory, 0, 0,
							ri.BytesPerRow,
							uidata->appWnd->appWindow->RPort,
							x,
							y,
							img->bitmaphdr.Width,img->bitmaphdr.Height,
							ri.RGBFormat);
		}else{
			goto cleanup;
		}
	}else{
		setViewPortColorTable(&uidata->data.ctx, &uidata->appWnd->appWindow->WScreen->ViewPort, uidata->currentPage->colourTable, uidata->maxDepth);
		WaitBlit();
		BltBitMapRastPort(bmpimg, 0, 0, uidata->appWnd->appWindow->RPort,
					x,
					y,
					img->bitmaphdr.Width,
					img->bitmaphdr.Height,0xC0);
	}
	
	
	ret = TRUE ;
	
cleanup:
	if (bmpimg){
		freeBitMap(&uidata->data.ctx, bmpimg, &img->bitmaphdr);
		bmpimg = NULL ;
	}
	if (ri.Memory){
		FreeVec(ri.Memory);
		ri.Memory = NULL;
	}
	return ret ;
}

static BOOL _printString(struct RastPort *rp, UWORD x, UWORD y, UWORD w, UWORD h, char *str, UWORD len)
{
	char *s, *start, *end;
	BOOL ignoreWS = TRUE, checkLen = FALSE, forceNL = FALSE, forcePrint = FALSE;
	struct TextExtent te ;
	UWORD tx=0, ty=0;
	
	tx = x;
	ty = y;
	Move(rp, tx, ty);
	start = end = s = str;
	while (s-str < len){
		switch(*s){
			case '\t':
				*s = ' ';
			case ' ':
			case '\n':
				if (ignoreWS){
					start = s+1;
				}else{
					checkLen = TRUE ;
				}
				if (*s == '\n'){
					forceNL = TRUE;
					forcePrint = TRUE;
				}
				break;
			case '\\':
				forcePrint = TRUE;
				break;
			default:
				if (ignoreWS){
					ignoreWS = FALSE;
					checkLen = TRUE ;
				}
				break;
		}
		if (s-str >= len){
			forcePrint = TRUE;
			forceNL = FALSE;
		}
		if (checkLen){
			checkLen = FALSE ;
			TextExtent(rp, start, s - start, &te);
			if (te.te_Width >= (w-(tx-x))){ // The text needs printing and new line
				//printf("TextExtent width %u, for str %c%c%c%c%c... tx offset %u, width %u\n", te.te_Width, *start,*(start+1),*(start+2),*(start+3),*(start+4), tx, w);
				forcePrint = TRUE ;
				forceNL = TRUE ;
			}else{
				end = s; // Not too long yet, unless forced to print anyway. Remeber last end point before too long
			}
			if (forcePrint){ // Must print now
				forcePrint = FALSE ;
				if (te.te_Width >= (w-(tx-x))){ // was too long so go to last end point
					s = end;
				}
				if (start < s){ // is there anything to print?
					Text(rp, start, s - start); // Print it
					//printf("Printing %p, %c%c%c%c%c... len %u\n", start, *start,*(start+1),*(start+2),*(start+3),*(start+4), s-start);
					start = s+1; // set start to be where we got to + 1
					tx += te.te_Width; // Move cursor forward
				}
				if (forceNL){
					forceNL = FALSE;
					ty += te.te_Height + te.te_Extent.MaxY;
					tx = x; // Move x cursor to start of line
					ignoreWS = TRUE; // clear any leading spaces
				}
				if (ty > (y+h)){
					// Exceeding bounds
					//printf("Exceed bounds y:%u h:%u, ty:%u\n", y, h, ty);
					if (s-str < len-1){
						return FALSE ; // Cannot print remaining chars
					}
					return TRUE ;
				}
				//printf("Move pointer to tx:%u, ty:%u\n", tx, ty);
				Move(rp, tx, ty);
			}
		}
		++s;
	}
	//printf("All done, s-str %u, len %u\n", s-str, len);
	return TRUE;
}

static BOOL _openPageText(struct MagUIData *uidata, struct MagText *text)
{
	UWORD x=0,y=0,w=0,h=0;
	char *textBody = NULL ;
	BOOL ret = FALSE ;
	
	if (text->config){
		x = magatouw(findValue("X", text->config), 0);
		y = magatouw(findValue("Y", text->config), 0);
		w = magatouw(findValue("WIDTH", text->config), uidata->appWnd->appWindow->Width - x);
		h = magatouw(findValue("HEIGHT", text->config), uidata->appWnd->appWindow->Height - y);
	}
	
	x += uidata->borderLeft;
	y += uidata->borderTop ;
	
	if (!(textBody = (char *)AllocVec(text->length, MEMF_CLEAR | MEMF_ANY))){
		goto cleanup;
	}
	D(printf("AllocVec: _openPageText MEM %p, size %u\n", textBody, text->length));

	fseek(uidata->data.ctx.f,text->offset,SEEK_SET);
	if (fread(textBody, text->length, 1, uidata->data.ctx.f) == 0){
		goto cleanup;
	}
	_printString(uidata->appWnd->appWindow->RPort, x, y, w, h, textBody, text->length);
	
	ret = TRUE ;
cleanup:
	if (textBody){
		FreeVec(textBody);
	}
	return ret ;
}

static BOOL _openMusic(struct MagUIData *uidata, struct MagPage *page, struct MagParameter *param, struct IFFMod *mod)
{
	struct MagValue *val = NULL ;
	
	uidata->modLoopCount = 0;
	uidata->modStopOnExit = TRUE ;
	
	if ((val=findValue("LOOP", param))){
		uidata->modLoopCount = magatouw(val,0);
	}
	if ((val=findValue("ONEXIT", param))){
		uidata->modStopOnExit = magstricmp("STOP", val->szValue, MAG_MAX_PARAMETER_VALUE) ;
	}
	
	if (uidata->activeMod){
		if (uidata->activeMod != mod){
			stopModMusic(uidata) ;
		}
	}
	
	if (!uidata->activeMod){
		startModMusic(uidata, mod);
	}
	
	return TRUE ;
}

static BOOL _openButton(struct MagUIData *uidata, struct MagPage *page, struct MagParameter *param)
{
	struct MagValue *val = NULL ;
	char *szName = NULL, *szNavTo = NULL ;
	UWORD x=0,y=0,w=0,h=0;
	struct MagControls *newBtn = NULL, *c = NULL ;
	ULONG tagDisable[] = {GA_Disabled, TRUE, TAG_END};
	ULONG tagEnable[] = {GA_Disabled, FALSE, TAG_END};
	
	if ((val=findValue("NAME", param))){
		szName = val->szValue ;
	}
	if ((val=findValue("NAVTO", param))){
		szNavTo = val->szValue ;
	}
	
	x = magatouw(findValue("X", param), 0);
	y = magatouw(findValue("Y", param), 0);
	w = magatouw(findValue("WIDTH", param), 100);
	h = magatouw(findValue("HEIGHT", param), 50);
	
	x += uidata->borderLeft;
	y += uidata->borderTop ;
	
	// Create button
	if (!(newBtn=AllocVec((sizeof(struct MagControls)), MEMF_ANY | MEMF_CLEAR))){
		return FALSE ; // no memory
	}
	
	// Link new button to list in uidata
	if (!uidata->ctls){
		uidata->ctls = newBtn;
	}else{
		for(c=uidata->ctls; c->next; c=c->next);
		c->next = newBtn;
	}

	newBtn->g.gadgetkind = BUTTON_KIND;
	newBtn->g.def.ng_LeftEdge = x;
	newBtn->g.def.ng_TopEdge = y;
	newBtn->g.def.ng_Width = w;
	newBtn->g.def.ng_Height = h;
	newBtn->g.def.ng_GadgetText = szName ;
	newBtn->g.def.ng_TextAttr = &topaz8;
	newBtn->g.def.ng_GadgetID = ++page->lastGadgetID;
	newBtn->g.def.ng_Flags = PLACETEXT_IN;
	newBtn->g.def.ng_VisualInfo = uidata->appWnd->app->visual;
	newBtn->g.data = newBtn ; // Cannot use ng_UserData as this is holding a pointer to the AppGadget object
	newBtn->param = param;
	newBtn->uidata = uidata;
	setGadgetTags(&newBtn->g, tagDisable);
	
	addAppGadget(uidata->appWnd, &newBtn->g) ;
	setGadgetTags(&newBtn->g, tagDisable);
	setGadgetTags(&newBtn->g, tagEnable);
	
	newBtn->fn_gadgetUp = _btnNav; // Default action
	if ((val=findValue("ACTION", param))){
		// Some standard actions - default is NAV if none set or unknown action
		if (magstricmp("EXIT", val->szValue, MAG_MAX_PARAMETER_VALUE)){
			newBtn->fn_gadgetUp = _btnCmdExit;
		}else if (magstricmp("NEXT", val->szValue, MAG_MAX_PARAMETER_VALUE)){
			newBtn->fn_gadgetUp = _btnNext;
		}else if (magstricmp("PREV", val->szValue, MAG_MAX_PARAMETER_VALUE)){
			newBtn->fn_gadgetUp = _btnPrev;
		}else{
			// Assume NavTo action
		}
	}
	return TRUE;
}

static UBYTE _hexToByte(char *str)
{
	char *p = str, c;
	UBYTE ret = 0, nibble = 1, val = 0;
	
	while (*p != '\0'){
		c = *p ;
		if ((c >= 'a' && c <= 'f')){
			c -= 32;
		}
		if ((c >= 'A' && c <= 'F')){
			val = c - 65 + 10 ;
		}else if ((c >= '0' && c <= '9')){
			val = c - 48;
		}else{
			return 0xFF ; // not a hex char
		}
		ret |= (val << (4*nibble));
		if(nibble == 0){
			break;
		}
		nibble--;
		p++ ;
	}
	return ret ;
}

static BOOL _convertStrToCol(struct MagColour *c, char *szColour)
{
	if (strlen(szColour) != 6){
		return FALSE;
	}
	c->r = _hexToByte(szColour);
	szColour += 2;
	c->g = _hexToByte(szColour);
	szColour += 2;
	c->b = _hexToByte(szColour);
	
	return TRUE;
}

__inline void _setMagColour(struct MagUIData *uidata, struct MagColour *col, UBYTE index)
{
	if (uidata->appWnd->app->gfx->lib_Version >= 39){
		SetRGB32(&uidata->appWnd->appWindow->WScreen->ViewPort, index, col->r << 24 | 0xFFFFFF,col->g << 24 | 0xFFFFFF,col->b << 24 | 0xFFFFFF);
	}else{
		SetRGB4(&uidata->appWnd->appWindow->WScreen->ViewPort, index, col->r, col->g, col->b);
	}
}

static void _setupPageColours(struct MagUIData *uidata, struct MagPage *page)
{
	struct MagParameter *pageParam = NULL ;
	struct MagValue *val = NULL ;
	
	// Setup colour defaults
	page->bg.r = 0x00;
	page->bg.g = 0x55;
	page->bg.b = 0xAA;
	
	page->pen.r = 0xFF;
	page->pen.g = 0xFF;
	page->pen.b = 0xFF;
	
	page->shadow.r = 0x00;
	page->shadow.g = 0x00;
	page->shadow.b = 0x34;
	
	page->highlight.r = 0xFF;
	page->highlight.g = 0x88;
	page->highlight.b = 0x00;
	
	
	if (pageParam=findParam("PAGE", &page->config.topSection, NULL)){
		if (val = findValue("BG", pageParam)){
			_convertStrToCol(&page->bg, val->szValue);
		}
		if (val = findValue("PEN", pageParam)){
			_convertStrToCol(&page->pen, val->szValue);
		}
		if (val = findValue("SHAD", pageParam)){
			_convertStrToCol(&page->shadow, val->szValue);
		}
		if (val = findValue("HIGH", pageParam)){
			_convertStrToCol(&page->highlight, val->szValue);
		}
	}
	_setMagColour(uidata, &page->bg, 0);
	_setMagColour(uidata, &page->shadow, 1);
	_setMagColour(uidata, &page->pen, 2);
	_setMagColour(uidata, &page->highlight, 3);
	SetAPen(uidata->appWnd->appWindow->RPort, 2);
	SetBPen(uidata->appWnd->appWindow->RPort, 0);
}

BOOL uiOpenPage(struct MagUIData *uidata, char *szPageRef)
{
	struct MagPage *page = NULL;
	struct IFFMod *mod = NULL ;
	struct MagParameter *param = NULL ;
	struct MagText *text = NULL ;
	struct IFFmaggfx *img = NULL;
	struct MagValue *val = NULL ;

	if (!(page=findPage(&uidata->data, szPageRef))){
		return FALSE ; // no page or any pages at all
	}
	if (uidata->currentPage){
		// Already have a page open, clear up
		if (uidata->currentPage->colourTable){
			FreeVec(uidata->currentPage->colourTable);
			uidata->currentPage->colourTable = NULL;
		}
	}
	if (IntuitionBase->LibNode.lib_Version >=39){
		SetWindowPointer(uidata->appWnd->appWindow, WA_BusyPointer, TRUE);
	}
	uidata->currentPage = page ;
	_setupPageColours(uidata, page);
	uiClearPage(uidata);
	
	// Setup all text
	for (text = findText(&page->config.topSection, NULL); text; text = findText(&page->config.topSection, (struct MagObject*)text)){
		_openPageText(uidata,text);
	}
	
	// Setup all buttons
	for (param = findParam("BUTTON", &page->config.topSection, NULL); param; param = findParam("BUTTON", &page->config.topSection, (struct MagObject*)param)){
		_openButton(uidata,page, param);
	}
	
	// Load in all sound/music
	for (param = findParam("MUSIC", &page->config.topSection, NULL); param; param = findParam("MUSIC", &page->config.topSection, (struct MagObject*)param)){
		if ((val=findValue("FILE", param))){
			if (mod = findMod(&uidata->data, val)){
				if (_openMusic(uidata, page, param, mod)){
					magRegisterTick(uidata, musicTickEvent);
				}
			}
		}
	}
	
	// Setup all images
	for (param = findParam("IMAGE", &page->config.topSection, NULL); param; param = findParam("IMAGE", &page->config.topSection, (struct MagObject*)param)){
		if ((val=findValue("NAME", param))){
			if (img = findImage(page, val)){
				_openPageImage(uidata, param, img);
			}
		}
	}
	
	if (param=findParam("PAGE", &page->config.topSection, NULL)){
		if (val = findValue("FX", param)){
			effectsInit(uidata, val->szValue);
		}
	}
	
	if (IntuitionBase->LibNode.lib_Version >=39){
		SetWindowPointer(uidata->appWnd->appWindow, WA_BusyPointer, FALSE);
	}
	return TRUE;
}

void uiClearPage(struct MagUIData *uidata)
{
	struct MagControls *n = NULL, *c = NULL;
	
	// Stop any intuiticks unless needed
	magUnregisterAllTicks(uidata);
	
	if (uidata->dbufBitmaps && uidata->appWnd->appWindow){
		uidata->dbufActive = 0;
		uidata->appWnd->appWindow->WScreen->ViewPort.RasInfo->BitMap = &uidata->dbufBitmaps[uidata->dbufActive];
		uidata->appWnd->appWindow->RPort->BitMap = &uidata->dbufBitmaps[uidata->dbufActive];
		MakeScreen(uidata->appWnd->appWindow->WScreen);
		RethinkDisplay();
	}
	
	// Clear any previous setup GELs/Bobs
	removeBobs(uidata->appWnd, &uidata->gels);
	
	// Check for any existing buttons and clean up
	if (uidata->ctls){
		c = uidata->ctls;
		while(c){
			n = c->next;
			delAppGadget(&c->g);
			FreeVec(c);
			c = n;
		}
		uidata->ctls = NULL;
	}
	
	// Clear background with configured pens
	if (uidata->appWnd->appWindow){
		EraseRect(uidata->appWnd->appWindow->RPort, 
					0,0, 
					uidata->appWnd->appWindow->Width, 
					uidata->appWnd->appWindow->Height);
	}
	
	// handle any music
	if (uidata->activeMod){
		if (uidata->modStopOnExit){
			stopModMusic(uidata);
		}else{
			magRegisterTick(uidata, musicTickEvent);
		}
	}
}