# Makefile for Magazine Application

SCOPTS = DEFINE=_DEBUG IGNORE=193 debug=full IncludeDirectory=/CGraphX/C/Include/ IncludeDirectory=/amigaui/
#SCOPTS = OPTIMIZE NoCheckAbort Optimizerinline OptimizerComplexity=10 OptimizerGlobal OptimizerDepth=1 OptimizerLoop OptimizerTime OptimizerSchedule OptimizerPeephole IGNORE=193 IncludeDirectory=/CGraphX/C/Include/ IncludeDirectory=/amigaui/

all: Mag MakeMag MakeSprite

clean:
	delete \#?.o \#?.lnk \#?.map \#?.gst Mag MakeMag TestConfig MakeSprite

MakeMag: makemag.o config.o
	sc link to MakeMag makemag.o config.o library=/amigaui/ui.lib
	
MakeSprite: makesprite.o
	sc link to MakeSprite makesprite.o library=/amigaui/ui.lib

Mag: magui.o magdata.o config.o maggfx.o magpages.o mageffects.o
	sc link to Mag magui.o magdata.o config.o maggfx.o magpages.o mageffects.o library=/amigaui/ui.lib

TestConfig: test.o config.o
   sc link to TestConfig test.o config.o
   
magui.o: magui.c 
   sc $(SCOPTS) magui.c
   
makemag.o: makemag.c 
   sc $(SCOPTS) makemag.c
  
makesprite.o: makesprite.c 
   sc $(SCOPTS) makesprite.c
   
magdata.o: magdata.c 
   sc $(SCOPTS) magdata.c
   
magpages.o: magpages.c 
   sc $(SCOPTS) magpages.c
   
maggfx.o: maggfx.c 
   sc $(SCOPTS) maggfx.c
   
test.o: test.c 
   sc $(SCOPTS) test.c

config.o: config.c 
   sc $(SCOPTS) config.c

mageffects.o: mageffects.c 
   sc $(SCOPTS) mageffects.c