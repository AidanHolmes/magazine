#include "iff.h"
#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <proto/exec.h>
#include <exec/exec.h>
#include <clib/icon_protos.h>
#include <clib/dos_protos.h>
#include <workbench/startup.h>

const char __ver[] = "$VER: MakeMag 1.0 (22.03.2025)";

FILE* loadParameters(char **szIFF, int argc, char **argv)
{
	char *szConfigFileName = NULL;
	struct WBStartup *WBenchMsg;
	struct WBArg *wbarg;
	struct DiskObject *dobj;
	UWORD i = 0;
	LONG olddir = -1;
	char *strToolValue ;
	struct Library *IconBase = NULL;
	FILE *f = NULL;
	
	if (argc >= 3){
	  szConfigFileName = argv[1];
	  *szIFF = argv[2];
	}else if (argc ==0){
		// Started from Workbench
		if(!(IconBase = OpenLibrary("icon.library",33))){
		 return NULL;
		}
		WBenchMsg = (struct WBStartup *)argv;
		for(i=0, wbarg=WBenchMsg->sm_ArgList; i < WBenchMsg->sm_NumArgs; i++, wbarg++){
			olddir = -1;
			if (wbarg->wa_Lock && *wbarg->wa_Name){
				olddir = CurrentDir(wbarg->wa_Lock);
			}
		 
			if((*wbarg->wa_Name) && (dobj=GetDiskObjectNew(wbarg->wa_Name))){
				if ((strToolValue = (char *)FindToolType((char **)dobj->do_ToolTypes, "CONFIG"))){
					szConfigFileName = strToolValue;
				}
				if ((strToolValue = (char *)FindToolType((char **)dobj->do_ToolTypes, "IFF"))){
					*szIFF = strToolValue;
				}
			}
		 
			if (olddir == -1){
				CurrentDir(olddir);
			}
		}
		CloseLibrary(IconBase);
	}
	
	if (!szConfigFileName || !szConfigFileName[0]){
		printf("No config file specified on command line or in CONFIG tool type\n");
		return NULL;
	}else{
		f=fopen(szConfigFileName, "r");
		if (!f){
			printf("Cannot open config file %s\n", szConfigFileName);
			return NULL ;
		}
	}
	return f ;
}

BOOL addMusicToCatalogue(struct IFFnode *cat, struct MagParameter *param)
{
	// Reads the config parameter, opens the file and adds the MOD to the CAT MODS.
	FILE *f = NULL;
	struct MagValue *val = NULL ;
	BOOL bRet = FALSE ;
	struct IFFnode *n = NULL, *form = NULL;
	
	if ((val=findValue("FILE", param))){
		if (!(f=fopen(val->szValue,"rb"))){
			// Cannot open this file
			printf("Cannot open music file %s\n", val->szValue);
			return FALSE ;
		}
	}else{
		printf("Music parameter has no FILE attribute. Aborting this reference\n");
		return FALSE ;
	}
	
	// Create new FORM for the mod music file. Copy all the content
	if (!(form=createIFFForm(cat, "MODS"))){
		printf("Error, cannot create new FORM in MODS\n");
		goto tidyup ;
	}
	
	if (!(n = createIFFChunk(form, "NAME"))){
		printf("Failed to create NAME chunk in new music FORM\n");
		goto tidyup;
	}
	n->bufContent = val->szValue;
	n->dataLength = strlen(val->szValue) +1; // add terminator
	
	if (!(n = createIFFChunk(form, "CSEQ"))){
		printf("Failed to create CSEQ chunk in new music FORM\n");
		goto tidyup;
	}
	n->fContent = f; // only need to set this value if it is the whole file
	
	bRet = TRUE;
tidyup:
	if (!bRet){
		if (f){
			fclose(f);
		}
		if (form){
			// remove failed form (if created).
			deleteIFFNode(form);
		}
	}
	return bRet;
}
	
BOOL addImageToList(struct IFFnode *list, struct IFFnode *prop, struct MagParameter *param)
{
	// Reads the config parameter, opens the file and adds the CMAP to the LIST PROP if 
	// it doesn't already exist. Creates a new ILBM form to the LIST
	FILE *f = NULL;
	struct IFFgfx gfx;
	struct MagValue *val = NULL ;
	BOOL bRet = FALSE ;
	struct IFFnode *cmap = NULL, *n = NULL, *form = NULL;
	UWORD ret = 0;
	
	// Any errors will just result in the file being excluded from the IFF. There's no other 
	// exception handling beyond just aborting the inclusion and returning FALSE.
	
	if ((val=findValue("FILE", param))){
		if (!(f=fopen(val->szValue,"rb"))){
			// Cannot open this file
			printf("Cannot open image file %s\n", val->szValue);
			return FALSE ;
		}
	}else{
		printf("Parameter has no FILE attribute. Aborting this reference\n");
		return FALSE ;
	}
	
	if (initialiseIFF(&gfx) != IFF_NO_ERROR){
		printf("Failed to initialise IFF parser\n");
		goto tidyup;
	}
	
	if ((ret=parseIFFImage(&gfx, f)) == IFF_NO_ERROR){
		// TO DO: extract CMAP and sections. Check the format is applicable for MAG images, etc
		if (gfx.cmap.length > 0){
			// Search prop container for a CMAP. If one hasn't been set then add (Assume other CMAPs are the same across images for this page)
			if (!(cmap=findNodeInContainer(prop, "CMAP"))){
				if (!(cmap = createIFFChunk(prop, "CMAP"))){
					printf("Failed to create CMAP chunk in LIST PROP\n");
					goto tidyup;
				}
				cmap->fContent = f;
				cmap->dataOffset = gfx.cmap.offset;
				cmap->dataLength = gfx.cmap.length;
			}
		}
		
		// Create new FORM for the image. Copy sections from parsed file into FORM
		if (!(form=createIFFForm(list, "ILBM"))){
			printf("Error, cannot create new FORM in image LIST\n");
			goto tidyup ;
		}
		// Copy BMHD
		if (gfx.bmhd.length > 0){
			if (!(n = createIFFChunk(form, "BMHD"))){
				printf("Failed to create BMHD chunk in new image FORM\n");
				goto tidyup;
			}
			n->fContent = f;
			n->dataOffset = gfx.bmhd.offset;
			n->dataLength = gfx.bmhd.length;
		}else{
			printf("Cannot find BMHD section in %s\n", val->szValue);
			goto tidyup;
		}
		// Copy CAMG (if exists)
		if (gfx.camg.length > 0){
			if (!(n = createIFFChunk(form, "CAMG"))){
				printf("Failed to create CAMG chunk in new image FORM\n");
				goto tidyup;
			}
			n->fContent = f;
			n->dataOffset = gfx.camg.offset;
			n->dataLength = gfx.camg.length;
		}
		// Copy BODY
		if (gfx.body.length > 0){
			if (!(n = createIFFChunk(form, "BODY"))){
				printf("Failed to create BODY chunk in new image FORM\n");
				goto tidyup;
			}
			n->fContent = f;
			n->dataOffset = gfx.body.offset;
			n->dataLength = gfx.body.length;
		}else{
			printf("Cannot find BODY section in %s\n", val->szValue);
			goto tidyup;
		}
		// Create custom chunks for the file
		// Create NAME chunk
		if ((val=findValue("NAME", param))){
			if (!(n = createIFFChunk(form, "NAME"))){
				printf("Failed to create NAME chunk in new image FORM\n");
				goto tidyup;
			}
			n->bufContent = val->szValue;
			n->dataLength = strlen(val->szValue) +1; // add terminator
		}else{
			printf("Parameter has no NAME attribute. Aborting this reference\n");
			goto tidyup;
		}
	}else{
		printf("Cannot parse image file %s\n", val->szValue);
		goto tidyup ;
	}
	
	bRet = TRUE;
tidyup:
	if (!bRet){
		if (f){
			fclose(f);
		}
		if (form){
			// remove failed form (if created).
			// note that CMAP could still exist in PROP
			deleteIFFNode(form);
		}
	}
	return bRet;
}	

struct IFFnode* createMag(struct MagConfig *config, char *szIFF)
{
	struct IFFnode *root = NULL, *prop = NULL, *propilbm = NULL, *n=NULL, *listilbm = NULL, *form = NULL, *modcat = NULL;
	BOOL error = TRUE ;
	struct MagParameter *param = NULL ;
	struct MagSection *thisSection = NULL ;
	struct MagValue *val = NULL ;
	FILE *fout = NULL;
	
	if (!(root = createIFFList(NULL, "MAG0"))){
		printf("Error creating root LIST\n");
		goto end;
	}
	
	if (!(prop=createIFFProperty(root, "MAG0"))){
		printf("Error creating PROP\n");
		goto end ;
	}
	
	// Look for properties to set at top level
	for (param = findParam("MAG", &config->topSection, NULL); param; param = findParam("MAG", &config->topSection, (struct MagObject*)param)){
		if ((val=findValue("AUTH", param))){
			if (!(n=createIFFChunk(prop,"AUTH"))){
				printf("Error, cannot create node AUTH in PROP\n");
				goto end ;
			}
			n->bufContent=val->szValue;
			n->dataLength = strlen(n->bufContent=val->szValue)+1;
		}
		if ((val=findValue("TITLE", param))){
			if (!(n=createIFFChunk(prop,"NAME"))){
				printf("Error, cannot create node NAME in PROP\n");
				goto end ;
			}
			n->bufContent=val->szValue;
			n->dataLength = strlen(n->bufContent=val->szValue)+1;
		}
		if ((val=findValue("COPYRIGHT", param))){
			if (!(n=createIFFChunk(prop,"(c) "))){
				printf("Error, cannot create node (c) in PROP\n");
				goto end ;
			}
			n->bufContent=val->szValue;
			n->dataLength = strlen(n->bufContent=val->szValue)+1;
		}
		if ((val=findValue("VERSION", param))){
			if (!(n=createIFFChunk(prop,"$VER"))){
				printf("Error, cannot create node $VER in PROP\n");
				goto end ;
			}
			n->bufContent=val->szValue;
			n->dataLength = strlen(n->bufContent=val->szValue)+1;
		}
	}
	
	// Next level down contains all page sections (any sub sections are ignored)
	// For each section/page, create a FORM with a LIST ILBM with a standard PROP section for the CMAP and other info
	for(thisSection=config->topSection.child; thisSection; thisSection = thisSection->next){
		// Create containing FORM for all LIST ILBMs and other sections
		if (!(form = createIFFForm(root, "MAG0"))){
			printf("Error creating FORM for new section\n");
			goto end;
		}
		// Create LIST within the FORM (for the page)
		if (!(listilbm = createIFFList(form, "ILBM"))){
			printf("Error creating ILBM LIST for new page\n");
			goto end;
		}
		// Within the LIST there needs to be a PROPerty chunk to hold the CMAP and other common info for images
		if (!(propilbm = createIFFProperty(listilbm, "ILBM"))){
			printf("Error creating ILBM LIST for new page\n");
			goto end;
		}
		// Extract all images
		for (param = findParam("IMAGE", thisSection, NULL); param; param = findParam("IMAGE", thisSection, (struct MagObject*)param)){
			// These images need taking apart and reassembling into this MAG IFF format
			// CMAPS to be stripped and custom chunks added where required
			addImageToList(listilbm, propilbm, param);
		}
		
		// If there is any music specified, then create a catalogue section and add
		// all music to the catalogue.
		if (findParam("MUSIC", thisSection, NULL)){
			if (!(modcat=createIFFCatalogue(form, "MODS"))){
				printf("Error creating MODS CAT for new page\n");
				goto end;
			}
			
			// Extract all music mods
			for (param = findParam("MUSIC", thisSection, NULL); param; param = findParam("MUSIC", thisSection, (struct MagObject*)param)){
				// This adds a mod file as-is to the IFF. 
				addMusicToCatalogue(modcat, param);
			}
		}

		if (!(n=createIFFChunk(form,"CONF"))){
			printf("Error, cannot create CONF chunk in FORM\n");
			goto end ;
		}
		if (config->f){
			n->fContent = config->f;
		}else{
			n->bufContent = config->configbuffer;
		}
		n->dataOffset = thisSection->offset;
		n->dataLength = thisSection->length;
	}
	
	// Open file to write IFF
	fout = fopen(szIFF, "wb");
	if (!fout){
		printf("Cannot create %s output file\n", szIFF);
		goto end ;
	}
	
	if (createIFF(fout, root) != 0){
		printf("Failed to create IFF\n");
		goto end;
	}
	
	error = FALSE ;
end:
	if (fout){
		fclose(fout);
		fout = NULL;
	}
	
	if(error){
		deleteIFFNode(root);
		root = NULL ;
	}
	return root ;
}


int main(int argc, char **argv)
{
	FILE *f = NULL ;
	struct MagConfig mconf;
	UWORD confret = 0;
	char *szIFF = NULL;
	struct IFFnode *root = NULL;
	int ret = 10;
	
	if (!(f=loadParameters(&szIFF, argc, argv))){
		printf("Cannot load parameters\n");
		goto cleanup;
	}
	
	if (magInitialiseFile(&mconf, f, 0) != 0){
		printf("Cannot initialise the mag config library\n");
		return 10;
	}
	
	if ((confret=parseConfig(&mconf, NULL)) != MAG_RETURN_NOERROR){
		printf("Cannot parse config, err %u\n", confret);
		goto cleanup;
	}
	
	root = createMag(&mconf, szIFF) ;
	if (!root){
		printf("Cannot create IFF file\n");
		goto cleanup;
	}
	
	ret = 0;
cleanup:
	if(root){
		deleteIFFNode(root);
	}
	magCleanup(&mconf);
	return ret;
}

/*
int main(int argc, char **argv)
{
	char *configdata = NULL ;
	FILE *fconfig = NULL, *fout = NULL;
	ULONG configLen = 0 ;
	struct IFFnode *root = NULL, *prop = NULL, *form = NULL, *n = NULL;
	
	if (argc == 0){
		printf("Run from command line\n");
		return 5;
	}
	
	if (argc < 2){
		printf("Usage: %s config-file\n");
		return 5;
	}
	
	if (!(fconfig=fopen(argv[1], "r"))){
		printf("Cannot open %s config file\n", argv[1]);
		return 5;
	}
	
	fseek(fconfig,0,SEEK_END);
	configLen = ftell(fconfig);
	fseek(fconfig,0,SEEK_SET);
	
	configdata = (char*)malloc(configLen);
	if (!configdata){
		printf("Cannot allocate memory to read configuration\n");
		goto end;
	}

	if (fread(configdata,configLen, 1, fconfig) == 0){
		printf("Error reading config file\n");
		goto end;
	}
	
	if (!(root = createIFFList(NULL, "MAG0"))){
		printf("Error creating root LIST\n");
		goto end;
	}
	
	if (!(prop=createIFFProperty(root, "MAG0"))){
		printf("Error creating PROP\n");
		goto end ;
	}

	if (!(n=createIFFChunk(prop,"AUTH"))){
		printf("Error, cannot create node in PROP\n");
		goto end ;
	}
	n->bufContent = "Aidan Holmes";
	n->dataLength = 13;
	
	if (!(form=createIFFForm(root, "MAG0"))){
		printf("Error creating FORM\n");
		goto end ;
	}
	
	if (!(n=createIFFNode(form))){
		printf("Error, cannot create node in FORM\n");
		goto end ;
	}
	if (!(n->fContent = fopen("PowerPortia.iff", "rb"))){
		printf("Cannot open image file\n");
		goto end;
	}
	n->literal = TRUE; // Include entire file as a chunk

	//buildConfig(configdata);
	
	fout = fopen("test.iff", "wb");
	if (!fout){
		printf("Cannot create test.iff output file\n");
		goto end ;
	}

	if (createIFF(fout, root) != 0){
		printf("Failed to create IFF\n");
		goto end;
	}
	
end:
	if (fout){
		fclose(fout);
	}
	if (root){
		deleteIFFNode(root);
	}
	if (configdata){
		free(configdata);
	}
	if (fconfig){
		fclose(fconfig);
	}
	return 0;
}
*/