# =============================================================================
#  Project:   Game Boy Emulator
#  File:      emulator.mak
#
#  Description:
#    Simple multi-target build system for the emulator.
#    Supports native and cross-compilation.
#
#  Usage:
#    make -sf emulator.mak TARGET=<windows|android> [DEBUG=0|1]
#
# =============================================================================

#-----------------------------------------------------------------------------#
#                                Target Setup                                 #
#-----------------------------------------------------------------------------#
# target:
# - windows
# - android
TARGET?=windows
OUTDIR=.release_$(TARGET)
DEBUG?=0
PER_PIXEL_DRAW?=0

#-----------------------------------------------------------------------------#
#                    Stuff to be executed unconditionally                     #
#-----------------------------------------------------------------------------#
PROJECT=GBC-Emulator
ifeq ($(MAKELEVEL),0)
$(info --------------------------)
$(info | $(PROJECT) - $(TARGET) |)
$(info --------------------------)
$(shell mkdir -p $(OUTDIR))
endif

#-----------------------------------------------------------------------------#
#                                 Toolchain                                   #
#-----------------------------------------------------------------------------#
ifeq ($(TARGET),windows)
COMPILER = /c/msys64/mingw64/bin
	CC = $(COMPILER)/gcc
	OBJDUMP = $(COMPILER)/objdump
	BIN	= $(COMPILER)/objcopy

	SDL_PATH=/c/libs/SDL2-2.32.0/x86_64-w64-mingw32
	SDL_INC=$(SDL_PATH)/include/SDL2
	SDL_LIB=$(SDL_PATH)/bin

	SDL_TTF_PATH=/c/libs/SDL2_ttf-2.24.0/x86_64-w64-mingw32
	SDL_TTF_INC=$(SDL_TTF_PATH)/include/SDL2
	SDL_TTF_LIB=$(SDL_TTF_PATH)/bin
endif

ifeq ($(TARGET),android)
	CC = aarch64-linux-android21-clang
endif

#-----------------------------------------------------------------------------#
#                                   Sources                                   #
#-----------------------------------------------------------------------------#
VPATH = src

SRC = \
		emulator.c \
		cpu.c \
		bus.c \
		joypad.c \
		timer.c \
		serial.c \
		apu.c \
		ppu.c \
		trace.c \
		ppu_debug.c

ifeq ($(TARGET),windows)
	VPATH += src/windows
	SRC += main.c
endif

OBJS = $(addprefix $(OUTDIR)/,$(SRC:.c=.o))

#-----------------------------------------------------------------------------#
#                            Compiler/Linker Flags                            #
#-----------------------------------------------------------------------------#
CFLAGS = \
		$(addprefix -I,$(VPATH)) \
		-DDEBUG=$(DEBUG) \
		-DUSE_0xE000_AS_PUTC_DEVICE=0 \
		-DPER_PIXEL_DRAW=$(PER_PIXEL_DRAW) \
		-ffunction-sections \
		-fdata-sections \
		-g \
		-O0

LDFLAGS = \
		-ffunction-sections \
		-fdata-sections \
		-Wl,-gc-sections

ifeq ($(TARGET),windows)
	CFLAGS += \
		-I$(SDL_INC) \
		-I$(SDL_TTF_INC) \
		-Wimplicit-fallthrough=3

	LDFLAGS += \
		-L$(SDL_LIB) \
		-L$(SDL_TTF_LIB) \
		-lSDL2 \
		-lSDL2_ttf
endif

ifeq ($(TARGET),android)
	CFLAGS += \
		-fPIC
	LDFLAGS += \
		-shared
endif
#-----------------------------------------------------------------------------#
#                                 Compilation                                 #
#-----------------------------------------------------------------------------#

# generate .o-files from c-files in src directory
$(OUTDIR)/%.o : %.c
	@echo "compiling $< ..."
	$(CC) $(CFLAGS) -c $< -o $@

ifeq ($(TARGET),windows)
.PHONY: all exe lss

exe: $(OUTDIR)/$(PROJECT).exe
lss: $(OUTDIR)/$(PROJECT).lss
all: exe lss

# generate .exe file from objects
$(OUTDIR)/$(PROJECT).exe: $(OBJS)
	@echo "generating $@ ..."
	$(CC) $(LDFLAGS) $(OBJS) -o $@
	cp $(SDL_LIB)/SDL2.dll $(OUTDIR)/SDL2.dll

# generate .lss file from .exe file
$(OUTDIR)/%.lss: $(OUTDIR)/%.exe
	@echo "generating $@ ..."
	$(OBJDUMP) -d -h -l $< > $@
# -h -l -S
# not used anymore but lets keep it just in case ...
# dll:
# 	$(CC) -m32 -static-libgcc -shared -DBUILD_TEST_DLL=1 -o $(OUTDIR)/$(PROJECT).dll $(SRC)
endif

ifeq ($(TARGET),android)
.PHONY: all so

so: $(OUTDIR)/$(PROJECT).so
all: so

# generate .so file from objects
$(OUTDIR)/$(PROJECT).so: $(OBJS)
	@echo "generating $@ ..."
	$(CC) $(LDFLAGS) $(OBJS) -o $@
endif

.PHONY: clean
clean:
	rm -rf $(OUTDIR)

#-----------------------------------------------------------------------------#
#                                     EOF                                     #
#-----------------------------------------------------------------------------#
