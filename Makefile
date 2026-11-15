#---------------------------------------------------------------------------------
# PocketPKMN (was WalkerBox) - 3DS homebrew, built with devkitARM + libctru.
# Based on the stock devkitPro 3DS application template.
# Build with build.ps1 (runs make inside the devkitpro/devkitarm container).
#---------------------------------------------------------------------------------
.SUFFIXES:

ifeq ($(strip $(DEVKITARM)),)
$(error "Please set DEVKITARM in your environment. export DEVKITARM=<path to>devkitARM")
endif

TOPDIR ?= $(CURDIR)
include $(DEVKITARM)/3ds_rules

TARGET		:=	pocketpkmn
BUILD		:=	build
PKSM		:=	lib/PKSM-Core
SOURCES		:=	source \
			$(PKSM)/source $(PKSM)/source/i18n $(PKSM)/source/personal $(PKSM)/source/pkx \
			$(PKSM)/source/sav $(PKSM)/source/utils $(PKSM)/source/wcx $(PKSM)/memecrypto
DATA		:=	data
INCLUDES	:=	include \
			$(PKSM)/include $(PKSM)/include/enums $(PKSM)/include/personal $(PKSM)/include/pkx \
			$(PKSM)/include/sav $(PKSM)/include/utils $(PKSM)/include/wcx $(PKSM)/memecrypto \
			$(PKSM)/pcg-cpp/include
GRAPHICS	:=	gfx
GFXBUILD	:=	$(BUILD)
# Run-time data for PKSM-Core (strings, species tables), mounted as romfs:/
ROMFS		:=	romfs

APP_TITLE	:=	PocketPKMN
APP_DESCRIPTION	:=	Pokewalker companion app
APP_AUTHOR	:=	Medarotter26

# CIA (make cia; cia/, docs/SESSION_CIA.md). Title ID 000400000F8F9E00 = 0x00040000 high, UniqueId 0xF8F9E.
APP_PRODUCT_CODE :=	CTR-H-PKPK
APP_UNIQUE_ID	:=	0xF8F9E
# makerom + bannertool are not in the devkitarm image and the downloaded Linux builds need a newer glibc than it
# has, so the packing runs in WSL itself: cia/make_cia.sh (build.ps1 cia does both steps).

#---------------------------------------------------------------------------------
# options for code generation
#---------------------------------------------------------------------------------
ARCH	:=	-march=armv6k -mtune=mpcore -mfloat-abi=hard -mtp=soft

CFLAGS	:=	-g -Wall -O2 -mword-relocations \
			-ffunction-sections \
			$(ARCH)

CFLAGS	+=	$(INCLUDE) -D__3DS__

# PKSM-Core throws in a few places (personal tables, newer generations), so C++
# is built with exceptions; it needs no RTTI.
CXXFLAGS	:= $(CFLAGS) -fno-rtti -fexceptions -std=gnu++20 -Wno-psabi

ASFLAGS	:=	-g $(ARCH)
LDFLAGS	=	-specs=3dsx.specs -g $(ARCH) -Wl,-Map,$(notdir $*.map)

LIBS	:= -lcitro2d -lcitro3d -lctru -lm

LIBDIRS	:= $(CTRULIB)

#---------------------------------------------------------------------------------
ifneq ($(BUILD),$(notdir $(CURDIR)))
#---------------------------------------------------------------------------------

export OUTPUT	:=	$(CURDIR)/$(TARGET)
export TOPDIR	:=	$(CURDIR)

export VPATH	:=	$(foreach dir,$(SOURCES),$(CURDIR)/$(dir)) \
			$(foreach dir,$(GRAPHICS),$(CURDIR)/$(dir)) \
			$(foreach dir,$(DATA),$(CURDIR)/$(dir))

export DEPSDIR	:=	$(CURDIR)/$(BUILD)

CFILES		:=	$(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.c)))
CPPFILES	:=	$(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.cpp)))
SFILES		:=	$(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.s)))
GFXFILES	:=	$(foreach dir,$(GRAPHICS),$(notdir $(wildcard $(dir)/*.t3s)))
BINFILES	:=	$(foreach dir,$(DATA),$(notdir $(wildcard $(dir)/*.*)))

ifeq ($(strip $(CPPFILES)),)
	export LD	:=	$(CC)
else
	export LD	:=	$(CXX)
endif

export T3XFILES :=  $(GFXFILES:.t3s=.t3x)

export OFILES_SOURCES 	:=	$(CPPFILES:.cpp=.o) $(CFILES:.c=.o) $(SFILES:.s=.o)

export OFILES_BIN	:=	$(addsuffix .o,$(BINFILES)) \
			$(addsuffix .o,$(T3XFILES))

export OFILES := $(OFILES_BIN) $(OFILES_SOURCES)

export HFILES	:=	$(addsuffix .h,$(subst .,_,$(BINFILES))) \
			$(GFXFILES:.t3s=.h)

export INCLUDE	:=	$(foreach dir,$(INCLUDES),-I$(CURDIR)/$(dir)) \
			$(foreach dir,$(LIBDIRS),-I$(dir)/include) \
			-I$(CURDIR)/$(BUILD)

export LIBPATHS	:=	$(foreach dir,$(LIBDIRS),-L$(dir)/lib)

export _3DSXDEPS	:=	$(if $(NO_SMDH),,$(OUTPUT).smdh)

ifneq ($(wildcard icon.png),)
	export APP_ICON := $(TOPDIR)/icon.png
else ifneq ($(wildcard cia/icon.png),)
	export APP_ICON := $(TOPDIR)/cia/icon.png
endif

ifeq ($(strip $(NO_SMDH)),)
	export _3DSXFLAGS += --smdh=$(CURDIR)/$(TARGET).smdh
endif

ifneq ($(ROMFS),)
	export _3DSXFLAGS += --romfs=$(CURDIR)/$(ROMFS)
endif

.PHONY: all clean cia

all: $(BUILD) $(DEPSDIR)
	@$(MAKE) --no-print-directory -C $(BUILD) -f $(CURDIR)/Makefile

$(BUILD):
	@mkdir -p $@

# pocketpkmn.cia from the same .elf and romfs/ as the .3dsx, with our own banner/jingle/icon (tools/cia_art.py).
# Only where makerom + bannertool run (CIA_TOOLS / MAKEROM / BANNERTOOL, as cia/make_cia.sh); with build.ps1 use
# ".\build.ps1 cia" instead. make does not notice romfs changes: delete pocketpkmn.3dsx/.cia after one.
cia: all
	@bash cia/make_cia.sh

clean:
	@echo clean ...
	@rm -fr $(BUILD) $(TARGET).3dsx $(OUTPUT).smdh $(TARGET).elf $(TARGET).cia

#---------------------------------------------------------------------------------
else
#---------------------------------------------------------------------------------

$(OUTPUT).3dsx	:	$(OUTPUT).elf $(_3DSXDEPS)

$(OFILES_SOURCES) : $(HFILES)

$(OUTPUT).elf	:	$(OFILES)

%.bin.o	%_bin.h :	%.bin
	@echo $(notdir $<)
	@$(bin2o)

.PRECIOUS	:	%.t3x
%.t3x.o	%_t3x.h :	%.t3x
	$(SILENTMSG) $(notdir $<)
	$(bin2o)

-include $(DEPSDIR)/*.d

#---------------------------------------------------------------------------------
endif
#---------------------------------------------------------------------------------
