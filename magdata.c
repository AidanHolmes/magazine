#include "magdata.h"
#include <stdio.h>
#include <string.h>
#include <proto/exec.h>
#include <exec/exec.h>

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

UWORD _parseIFFMag_callback(struct IFFstack *stack, struct IFFctx *ctx, ULONG offset)
{
	struct IFFMagazineData *iff = NULL;
	struct MagPage *thisPage = NULL ;
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
}

void cleanUpMagData(struct IFFMagazineData *iff)
{
	struct MagPage *page = NULL, *tmppage = NULL;
	struct IFFmaggfx *img = NULL, *tmpimg = NULL ;
	
	for(page=iff->pages;page;){
		tmppage = page->next;
		for(img=page->images;img;){
			tmpimg = img->next;
			FreeVec(img);
			img = tmpimg;
		}
		if (page->colourTable){
			FreeVec(page->colourTable);
		}
		magCleanup(&page->config);
		FreeVec(page);
		page = tmppage;
	}
	iff->pages = NULL ;
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