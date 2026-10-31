ifeq ($(strip $(PVSNESLIB_HOME)),)
export PVSNESLIB_HOME := $(CURDIR)/.tools/pvsneslib
endif

export ROMNAME := bombadventure
export ROMTITLE := BOMBADVENTURE
export SRC := src
export FASTROM := 1

include ${PVSNESLIB_HOME}/devkitsnes/snes_rules

.PHONY: all gfx test site

all: gfx $(ROMNAME).sfc

GFX_PNG := gfx/tiles.png gfx/flames.png gfx/font.png gfx/p1.png gfx/p2.png gfx/p3.png gfx/p4.png
GFX_PIC := $(GFX_PNG:.png=.pic) gfx/title.pic

gfx/.stamp: tools/mkgfx.py assets/font.bmp assets/flames.png $(wildcard assets/woka/*.png assets/sprites/*.png)
	python3 tools/mkgfx.py assets gfx
	touch $@

$(GFX_PNG): gfx/.stamp

gfx/tiles.pic: gfx/tiles.png
	$(GFXCONV) -s 8 -o 16 -u 16 -p -i $<

gfx/flames.pic: gfx/flames.png
	$(GFXCONV) -s 8 -o 16 -u 16 -p -i $<

gfx/font.pic: gfx/font.png
	$(GFXCONV) -s 8 -o 4 -u 4 -p -i $<

gfx/title.pic: tools/mktitle.py assets/title_bg.png assets/title_logo.png
	python3 tools/mktitle.py assets gfx

gfx/p%.pic: gfx/p%.png
	$(GFXCONV) -s 32 -o 16 -u 16 -p -i $<

SFX_WAV := sfx/bomb.wav sfx/boom.wav sfx/power.wav sfx/death.wav sfx/join.wav
SFX_BRR := $(SFX_WAV:.wav=.brr)

gfx: $(GFX_PIC) $(SFX_BRR)

sfx/.stamp: tools/mksfx.py
	python3 tools/mksfx.py sfx
	touch $@

$(SFX_WAV): sfx/.stamp

sfx/%.brr: sfx/%.wav
	$(BRCONV) -e $< $@

data.obj: $(GFX_PIC) $(SFX_BRR)

test: all
	python3 tests/smoke.py

# the GitHub Pages site: web player, ROM and title picture
site: all
	mkdir -p _site
	cp web/index.html _site/
	cp $(ROMNAME).sfc _site/
	cp gfx/title_preview.png _site/title.png
