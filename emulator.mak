# Makefile for GBC-Emulator

TARGET=emulator

COMPILER = /c/MinGW/bin
CC = $(COMPILER)/gcc
OBJDUMP = $(COMPILER)/objdump
BIN	= $(COMPILER)/objcopy

OUTDIR = .release

DEBUG?=0

ifeq ($(MAKELEVEL),0)
	useless := $(shell echo -e "Make Emulator")
	useless := $(shell mkdir -p $(OUTDIR))
endif

SRC = cpu.c

OBJS = $(addprefix $(OUTDIR)/,$(SRC:.c=.o))

CFLAGS = \
		-DDEBUG=$(DEBUG) \
		-DUSE_0xE000_AS_PUTC_DEVICE=1 \
		-ffunction-sections \
		-fdata-sections \
		-g \
		-O2

LDFLAGS = \
		-ffunction-sections \
		-fdata-sections \
		-Wl,-gc-sections

.PHONY: clean all exe lss

exe: $(OUTDIR)/$(TARGET).exe
lss: $(OUTDIR)/$(TARGET).lss
all: exe lss

# generate .o-files from c-files in src directory
$(OUTDIR)/%.o : %.c
	@echo "compiling $< ..."
	$(CC) $(CFLAGS) -c $< -o $@

# generate .elf file from objects
$(OUTDIR)/$(TARGET).elf: $(OBJS)
	$(CC) $(LDFLAGS) $(OBJS) -o $@

# generate .lss file from .elf file
$(OUTDIR)/%.lss: $(OUTDIR)/%.elf
	@echo "generating $@ ..."
	$(OBJDUMP) -D $< > $@

# -h -l -S

# generate .bin file from .elf file
$(OUTDIR)/%.exe: $(OUTDIR)/%.elf
	@echo "generating $@ ..."
	$(BIN) -O binary $< $@

dll:
	$(CC) -m32 -static-libgcc -shared -DBUILD_TEST_DLL=1 -o $(OUTDIR)/$(TARGET).dll $(SRC)

clean:
	rm -rf $(OUTDIR)
