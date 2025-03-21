#include "magdata.h"
#include "magui.h"
#include "gfx.h"
#include <stdio.h>
#include <string.h>
#include <proto/exec.h>
#include <exec/exec.h>

#define SCROLLTXT_BUF_MULT 3

struct MagPage* _createPage(void)
{
	return (struct MagPage*)AllocVec(sizeof(struct MagPage), MEMF_ANY | MEMF_CLEAR);
}

struct MagPage* _getLastPage(struct IFFMagazineData *iff)
{
	struct MagPage *thisPage = NULL;
	if (!iff->pages){
		return NULL;
	}
	for(thisPage=iff->pages; thisPage->next; thisPage=thisPage->next);
	
	return thisPage;
}

struct IFFmaggfx* _getLastImage(struct MagPage *page)
{
	struct IFFmaggfx *thisImg = NULL;
	if (!page->images){
		return NULL;
	}
	for(thisImg=page->images; thisImg->next; thisImg=thisImg->next);
	
	return thisImg;
}

struct MagScrollText* _getLastScrollText(struct MagPage *page)
{
	struct MagScrollText *thisTxt = NULL;
	if (!page->scrollText){
		return NULL;
	}
	for(thisTxt=page->scrollText; thisTxt->next; thisTxt=thisTxt->next);
	
	return thisTxt;
}

struct IFFmaggfx *_createImage(struct MagPage *page)
{
	struct IFFmaggfx *img = NULL, *attachTo = NULL;
	img = (struct IFFmaggfx*)AllocVec(sizeof(struct IFFmaggfx), MEMF_ANY | MEMF_CLEAR);

	if (img){
		img->page = page;
		if (!page->images){
			page->images = img;
		}else{
			if (attachTo = _getLastImage(page)){
				attachTo->next = img;
			}
		}
	}
	
	return img;
}

struct IFFMod* _getLastMod(struct IFFMagazineData *iff)
{
	struct IFFMod *thisMod = NULL;
	if (!iff->mods){
		return NULL;
	}
	for(thisMod=iff->mods; thisMod->next; thisMod=thisMod->next);
	
	return thisMod;
}

static struct IFFMod *_createMod(struct IFFMagazineData *iff)
{
	struct IFFMod *mod = NULL, *attachTo = NULL;
	mod = (struct IFFMod*)AllocVec(sizeof(struct IFFMod), MEMF_ANY | MEMF_CLEAR);

	if (mod){
		if (!iff->mods){
			iff->mods = mod;
		}else{
			if (attachTo = _getLastMod(iff)){
				attachTo->next = mod;
			}
		}
	}
	
	return mod;
}	

UWORD _parseIFFMag_callback(struct IFFstack *stack, struct IFFctx *ctx, ULONG offset)
{
	struct IFFMagazineData *iff = NULL;
	struct MagPage *thisPage = NULL ;
	struct IFFMod *thisMod = NULL ;
	struct IFFmaggfx *thisImg = NULL ;
	void *to = NULL;
	ULONG readSize = 0;
	
	iff = (struct IFFMagazineData*)ctx;
	
	///////////////////////////////////////////////
	//
	// Read New Page Chunk
	//
	if (CMP_HDR(stack->chunk_name, "FORM") && CMP_HDR(stack->chunk_id, "MAG0")){
		if (!iff->pages){
			if(!(iff->pages = _createPage())){
				return IFF_NORESOURCE_ERROR;
			}
		}else{
			if (thisPage = _getLastPage(iff)){
				if (!(thisPage->next = _createPage())){
					return IFF_NORESOURCE_ERROR;
				}
				thisPage->next->prev = thisPage; // link the prev pointer
			}
		}
		iff->pageCount++;
	}
	
	///////////////////////////////////////////////
	//
	// Read Configuration Chunk
	//
	else if (CMP_HDR(stack->chunk_name, "CONF")){
		// Validate that this configuration section is within a FORM MAG0
		if (stack->parent && CMP_HDR(stack->parent->chunk_name, "FORM") && CMP_HDR(stack->parent->chunk_id, "MAG0")){
			// Get current page, which is assumed to be last page created
			if (thisPage = _getLastPage(iff)){
				fseek(ctx->f, offset, SEEK_SET); // no need to remember as the parser will restore original file pointer.
				// Read the config into the page configuration value
				if (magInitialiseFile(&thisPage->config, ctx->f, stack->length) != MAG_RETURN_NOERROR){
					return IFF_NORESOURCE_ERROR; // assume resource issues
				}
				// Parse and structure configuration
				if (parseConfig(&thisPage->config, NULL) != MAG_RETURN_NOERROR){
					return IFF_NORESOURCE_ERROR; // assume resource issues
				}
			}
		}
	}
	
	///////////////////////////////////////////////
	//
	// Read Cmap from List Property
	//
	else if (CMP_HDR(stack->chunk_name, "CMAP")){
		// Check the property is within a list of properties or against the image (set all that apply)
		if (stack->parent && (thisPage = _getLastPage(iff))){
			if (CMP_HDR(stack->parent->chunk_name, "PROP") && CMP_HDR(stack->parent->chunk_id, "ILBM")){
				thisPage->cmap.offset = offset;
				thisPage->cmap.length = stack->length;
			}else if(CMP_HDR(stack->parent->chunk_name, "FORM") && CMP_HDR(stack->parent->chunk_id, "ILBM")){
				if (thisImg = _getLastImage(thisPage)){
					thisImg->cmap.offset = offset;
					thisImg->cmap.length = stack->length;
				}
			}
		}
	}
	
	///////////////////////////////////////////////
	//
	// Read new image
	//
	else if(CMP_HDR(stack->chunk_name, "FORM") && CMP_HDR(stack->chunk_id, "ILBM") && (thisPage = _getLastPage(iff))){
		// Create a new image object
		if (!(thisImg = _createImage(thisPage))){
			return IFF_NORESOURCE_ERROR;
		}
		thisPage->imageCount++ ;
	// Check parent object is a FORM and ILBM for the chunk
	}else if(stack->parent && CMP_HDR(stack->parent->chunk_name, "FORM") && CMP_HDR(stack->parent->chunk_id, "ILBM") && (thisPage = _getLastPage(iff))){
		if (CMP_HDR(stack->chunk_name, "BMHD")){
			if (thisImg = _getLastImage(thisPage)){
				thisImg->bmhd.offset = offset;
				thisImg->bmhd.length = stack->length;
				readSize = sizeof(struct _BitmapHeader);
				to = &thisImg->bitmaphdr;
			}
		}else if(CMP_HDR(stack->chunk_name, "BODY")){
			if (thisImg = _getLastImage(thisPage)){
				thisImg->body.offset = offset;
				thisImg->body.length = stack->length;
			}
		}else if(CMP_HDR(stack->chunk_name, "CAMG")){
			if (thisImg = _getLastImage(thisPage)){
				thisImg->camg.offset = offset;
				thisImg->camg.length = stack->length;
				readSize = 4;
				to = &thisImg->camg_flags;
			}
		}else if(CMP_HDR(stack->chunk_name, "NAME")){
			if (thisImg = _getLastImage(thisPage)){
				readSize = stack->length;
				to = &thisImg->szName;
			}
		}
		
		// Handle any buffer reads into image parameters
		if (to){
			if ((fread(to, 1, readSize, ctx->f)) != readSize){
				return IFF_STREAM_ERROR; // Cannot read
			}
		}
	}
	
	///////////////////////////////////////////////
	//
	// Read new music file
	//
	else if(CMP_HDR(stack->chunk_name, "FORM") && CMP_HDR(stack->chunk_id, "MODS")){
		// Create a new image object
		if (!(thisMod = _createMod(iff))){
			return IFF_NORESOURCE_ERROR;
		}
		iff->modCount++;
	// Check parent object is a FORM and MODS for the chunk
	}else if(stack->parent && CMP_HDR(stack->parent->chunk_name, "FORM") && CMP_HDR(stack->parent->chunk_id, "MODS")){
		if (CMP_HDR(stack->chunk_name, "NAME")){
			if (thisMod = _getLastMod(iff)){
				if ((fread(thisMod->szName, 1, stack->length, ctx->f)) != stack->length){
					return IFF_STREAM_ERROR; // Cannot read
				}
			}
		}else if(CMP_HDR(stack->chunk_name, "CSEQ")){
			if (thisMod = _getLastMod(iff)){
				thisMod->sequenceData.offset = offset;
				thisMod->sequenceData.length = stack->length;
			}
		}
	}
	
	return IFF_NO_ERROR;
}

UWORD parseIFFMagazine(struct IFFMagazineData *iff, FILE *f)
{
	iff->f = f;
	return parseIFF((struct IFFctx*)iff, _parseIFFMag_callback);
}

void initMagData(struct IFFMagazineData *iff)
{
	memset(iff, 0, sizeof(struct IFFMagazineData));
	initialiseIFFCtx((struct IFFctx *)iff);
}

void cleanUpMagData(struct IFFMagazineData *iff)
{
	struct MagPage *page = NULL, *tmppage = NULL;
	struct IFFMod *mod = NULL, *tmpmod = NULL;
	struct IFFmaggfx *img = NULL, *tmpimg = NULL ;
	
	for(page=iff->pages;page;){
		tmppage = page->next;
		for(img=page->images;img;){
			tmpimg = img->next;
			FreeVec(img);
			img = tmpimg;
		}
		page->images = NULL;

		removeAllScrollText(page);

		if (page->colourTable){
			FreeVec(page->colourTable);
		}
		magCleanup(&page->config);
		FreeVec(page);
		page = tmppage;
	}
	for (mod=iff->mods;mod;){
		tmpmod = mod->next;
		FreeVec(mod);
		mod = tmpmod ;
	}
	iff->mods = NULL ;
	iff->modCount = 0;
	iff->pages = NULL ;
	iff->pageCount = 0;
	closeIFFCtx((struct IFFctx*)iff);
}

struct MagScrollText *addScrollText(struct MagUIData *uidata, char *txt, UWORD len, UWORD x, UWORD y, UWORD w, UWORD h, UWORD speed, UWORD pen)
{
	struct MagScrollText *sTxt = NULL, *attachTo = NULL;
	struct MagPage *page = NULL ;
	char *szTxt = NULL ;
	BOOL ret = FALSE ;
	struct TextFont *tf = NULL;
	struct TextAttr __topaz8 = {
	   (STRPTR)"topaz.font", 8, 0, 1
	};

	if (!(page=uidata->currentPage)){
		return NULL;
	}

	sTxt = (struct MagScrollText*)AllocVec(sizeof(struct MagScrollText), MEMF_ANY | MEMF_CLEAR);

	if (sTxt){
		if (!(szTxt = (char*)AllocVec(len, MEMF_ANY | MEMF_CLEAR))){
			goto cleanup;
		}

		sTxt->page = page;
		sTxt->txt = szTxt;
		memcpy(szTxt, txt, len);
		sTxt->length = len;
		sTxt->x = x;
		sTxt->y = y;
		sTxt->height = h;
		sTxt->width = w;
		sTxt->speed = speed;
		sTxt->pen = pen;
		if (!(sTxt->backgnd = v36AllocBitMap(w,h,uidata->maxDepth))){
		//if (!(sTxt->backgnd = AllocBitMap(w,h,8,0,uidata->appWnd->appWindow->RPort->BitMap))){
			goto cleanup;
		}
		InitRastPort(&sTxt->textRastPort);
		if ((tf = OpenFont(&__topaz8))){
			SetFont(&sTxt->textRastPort,tf);
			CloseFont(tf);
		}
		//if (!(sTxt->textRastPort.BitMap = AllocBitMap(w,h*SCROLLTXT_BUF_MULT,8,0,uidata->appWnd->appWindow->RPort->BitMap))){
		if (!(sTxt->textRastPort.BitMap = v36AllocBitMap(w,h*SCROLLTXT_BUF_MULT,uidata->maxDepth))){
			goto cleanup;
		}
		
		if (!page->scrollText){
			page->scrollText = sTxt;
		}else{
			if (attachTo = _getLastScrollText(page)){
				attachTo->next = sTxt;
			}
		}
	}
	
	ret = TRUE ;
	
cleanup:
	if (!ret){
		if (sTxt){
			if (sTxt->backgnd){
				v36FreeBitMap(sTxt->backgnd, sTxt->width, sTxt->height);
				//FreeBitMap(sTxt->backgnd);
				sTxt->backgnd = NULL ;
			}
			if (sTxt->textRastPort.BitMap){
				v36FreeBitMap(sTxt->textRastPort.BitMap, sTxt->width, sTxt->height*SCROLLTXT_BUF_MULT);
				//FreeBitMap(sTxt->textRastPort.BitMap);
				sTxt->textRastPort.BitMap = NULL ;
			}
			if (sTxt->txt){
				FreeVec(sTxt->txt);
				sTxt->txt = NULL ;
			}

			FreeVec(sTxt);
			sTxt = NULL;
		}
	}
	
	return sTxt;
}

void removeAllScrollText(struct MagPage *page)
{
	struct MagScrollText *txt = NULL, *tmptxt = NULL ;
	for(txt=page->scrollText;txt;){
		if (txt->backgnd){
			v36FreeBitMap(txt->backgnd, txt->width, txt->height);
			txt->backgnd = NULL;
		}
		if (txt->txt){
			FreeVec(txt->txt);
			txt->txt = NULL;
		}
		if (txt->textRastPort.BitMap){
			v36FreeBitMap(txt->textRastPort.BitMap, txt->width, txt->height*SCROLLTXT_BUF_MULT);
			txt->textRastPort.BitMap = NULL ;
		}
		tmptxt = txt->next;
		FreeVec(txt);
		txt = tmptxt;
	}
	page->scrollText = NULL;
}

struct MagPage *findPage(struct IFFMagazineData *iff, char *szName)
{
	struct MagPage *page = NULL ;
	struct MagParameter *pageParam = NULL;
	struct MagValue *val = NULL;
	
	if (szName == NULL){
		return iff->pages;
	}
	
	for (page=iff->pages; page; page=page->next){
		if (pageParam=findParam("PAGE", &page->config.topSection, NULL)){
			if (val = findValue("REF", pageParam)){
				if (magstricmp(szName, val->szValue, MAG_MAX_PARAMETER_VALUE)){
					return page;
				}
			}
		}
	}
	return NULL;
}

struct IFFmaggfx *findImage(struct MagPage *page, struct MagValue *imgName)
{
	struct IFFmaggfx *img = NULL ;
	
	if (imgName == NULL){
		return NULL;
	}
	
	for (img=page->images; img; img=img->next){
		if (magstricmp(img->szName, imgName->szValue, MAG_MAX_PARAMETER_VALUE)){
			return img;
		}
	}
	return NULL;
}

struct IFFMod *findMod(struct IFFMagazineData *iff, struct MagValue *modName)
{
	struct IFFMod *mod = NULL ;
	
	if (modName == NULL){
		return NULL;
	}
	
	for (mod=iff->mods; mod; mod=mod->next){
		if (magstricmp(mod->szName, modName->szValue, MAG_MAX_PARAMETER_VALUE)){
			return mod;
		}
	}
	return NULL;
}