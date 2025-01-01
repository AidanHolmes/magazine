# About
Amiga Disk Magazine intended to show images, text, some effects and play mod music.
Written for use by the Robin Hood Amiga Group (RHAG). 

The app is Workbench friendly and doesn't take over the system to run. Instead it is intended to run in a window or screen alongside other apps.
This can mean it's a little less slick than a dedicated app. 

Disk content is built into an IFF file which contains all images, sounds, text and layout info.
As much as I would like to be fully standardised with EAs registry, the content chunks are quite custom. ILBM format is pretty much the only standard used from the registry.

PtPlayer by Frank Wille has been used to provide MOD playback. 

# Support
Mag application runs on V36 Amiga OS and later. It works on OCS, ECS, AGA and RTG systems.
The capabilities are worked out according to hardware registers and versions of libraries.
1 Mb of chip memory required to run. The app opens a PAL 640x512 pixel screen or window. 

# Building
The build system isn't that slick and will need some dependencies installing.

The following will be required:
- Ptplayer (https://aminet.net/package/mus/play/ptplayer)
- SDI headers (http://aminet.net/dev/c/SDI_headers.lha)
- AmigaUI library (https://github.com/AidanHolmes/amigaui)
- Cybergraphics library (https://aminet.net/package/dev/misc/CGraphX-DevKit)

Existing makefiles are for SAS/C smake. Includes are assumed to be in a top level lib directory. AmigaUI also defaulted to top level directory alongside magazine.
You can alter these in the makefiles. 

PtPlayer cannot be built with SAS/C and I've built with VASM and added to a lib with OML.
Build lines look as follows:

```
ptplayer.lib: ptplayer.o
	oml $@ $?
	
ptplayer.o: ptplayer.asm
	vasmm68k_mot -Fhunk -DOSCOMPAT=1 -DSDATA $? -o $@
```

# Applications
## Mag
This accepts a magazine IFF input on the command line or through a Workbench info file.

`Mag filename [screen|window]`

optional parameter to show the magazine in a 640x512 window or a dedicated screen. 
Window mode is intended for larger RTG resolutions and isn't double buffered. It will work on non-RTG but a bit pointless to use.

## MakeMag
This creates a magazine IFF file from a config file.

`MakeMag config output.iff`

Config is a text file. All referenced images and mods are relative to the directory run in.
The tool is solely to build the IFF and isn't optimised well for memory use or speed. It may need a few Mbs of memory for larger magazine files.

## MakeSprite
Used to define sprite headers (I use for Bob creation). It's a tool for the main build.
 
`Makesprite iffimage planes spritename outputfile`

Inputs an IFF file created in a painting app (Dpaint or PSP). 
Planes should set the number required for Bob/Sprite.
Spritename is a prefix in the output. Can be anything.
Outputfile is the header output to generate. 

# TO DO
## Vector graphics 
Support would be great. This should help cut down space required for raster images.
## Alternative buttons
The app uses standard gadgets for buttons. Whilst these work well, they are not easy to control colours and add other images. 
## Text scroll
Just like in demos. Scrolling text instead of boring static text. 
## Other effects
Image fades, screen transisitions and other effects such as star fields. 