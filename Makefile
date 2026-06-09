HEAP_SIZE      = 8388208
STACK_SIZE     = 61800

PRODUCT = AutoCrank.pdx

SDK = ${PLAYDATE_SDK_PATH}
ifeq ($(SDK),)
SDK = $(shell egrep '^\s*SDKRoot' ~/.Playdate/config | head -n 1 | cut -c9-)
endif

ifeq ($(SDK),)
$(error SDK path not found; set ENV value PLAYDATE_SDK_PATH)
endif

SRC = src/main.c src/game.c src/render.c src/images.c src/pets.c \
      src/triggers.c src/shop.c src/battle.c src/ai.c src/ui.c \
      src/sound.c src/save.c src/selftest.c src/creatures.c \
      src/names.c src/theme.c

UINCDIR = src

UDEFS = -DDEBUG_BUILD=1

include $(SDK)/C_API/buildsupport/common.mk

# common.mk's simulator dylib rule only tracks .c files; add headers so a
# header-only edit forces a dylib rebuild.
SRCHDR = $(wildcard src/*.h)
build/pdex.dylib: $(SRCHDR)

# common.mk's simulator (dylib) rule omits $(UDEFS), so project defines like
# DEBUG_BUILD never reach simulator builds — only the device build. Mirror
# UDEFS into the sim compiler flags so both builds agree (e.g. the DEBUG_BUILD
# gating of the dev system-menu tools). A release build flips UDEFS and both follow.
CLANGFLAGS += $(UDEFS)
GCCFLAGS   += $(UDEFS)

run: all
	open -a "$(SDK)/bin/Playdate Simulator.app" $(PRODUCT)
