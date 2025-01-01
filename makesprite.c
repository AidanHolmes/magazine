#include "iff.h"
#include <stdio.h>
#include <stdlib.h>


int main(int argc, char **argv)
{
	struct IFFgfx gfx;
	struct BitMap *bmp = NULL;
	WORD planes = 2, i=0, row=0, offset=0;
	FILE *fiff = NULL, *fout = NULL;
	UWORD *data;
	BOOL first = TRUE;
	char *szInputFile, *szSpriteName, *szOutputFile;
	
	if (argc < 5){
		printf("Usage: makesprite iffimage planes spritename outputfile\n");
		return 0;
	}
	szInputFile = argv[1];
	planes = atoi(argv[2]);
	szSpriteName = argv[3];
	szOutputFile = argv[4];
	
	if (!(fiff=fopen(szInputFile, "r"))){
		printf("Cannot open %s\n", szInputFile);
		return 0;
	}
	
	if (initialiseIFF(&gfx) != IFF_NO_ERROR){
		printf("Failed to initialise IFF library\n");
		return 0;
	}
	
	if (parseIFFImage(&gfx, fiff) != IFF_NO_ERROR){
		printf("Cannot parse %s as an IFF\n", argv[1]); 
		return 0;
	}
	
	if (!(bmp=createBitMap(&gfx.ctx, &gfx.body, &gfx.bitmaphdr, NULL))){
		printf("Cannot allocate bitmap from image\n");
		return 0;
	}
	
	if (!(fout=fopen(szOutputFile, "w"))){
		printf("Cannot create output file %s\n", szOutputFile);
		goto cleanup;
	}
	
	if (bmp->Depth < planes){
		printf("Image depth %u is smaller than requested planes %u\n", bmp->Depth, planes);
		goto cleanup;
	}

	fprintf(fout, "UWORD chip %s_sprite_data[%u * %u * %u] = { /* Planes * Word Width * Height */\n", szSpriteName, planes, bmp->BytesPerRow/2, bmp->Rows);
	for(i=0; i<planes;i++){
		fprintf(fout,"    /* Plane %u */\n    ", i);
		for (row=0;row<bmp->Rows;row++){
			data = (UWORD*)(bmp->Planes[i] + (bmp->BytesPerRow * row));
			for(offset=0;offset<bmp->BytesPerRow;offset += 2){
				if (!first){
					fprintf(fout,",");
				}else{
					first=FALSE;
				}
				fprintf(fout,"0x%04X",*data++);
			}
			fprintf(fout,"\n    ");
		}
		fprintf(fout,"\n");
	}
	fprintf(fout,"};\nconst UWORD %s_sprite_planes = %u;\n", szSpriteName, planes);
	fprintf(fout,"const UWORD %s_sprite_word_width = %u;\n", szSpriteName, bmp->BytesPerRow/2);
	fprintf(fout,"const UWORD %s_sprite_height = %u;\n", szSpriteName, bmp->Rows);
	
cleanup:
	if (bmp){
		freeBitMap(&gfx.ctx,bmp,&gfx.bitmaphdr);
	}
	if (fiff){
		fclose(fiff);
	}
	if (fout){
		fclose(fout);
	}
	return 0;
}