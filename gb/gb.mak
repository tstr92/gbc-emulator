# Makefile gb

TARGET=gb

COMPILER = "/c/Program Files (x86)/SDCC/bin"
MAKEBIN = $(COMPILER)/makebin
CC = $(COMPILER)/sdcc
ASM = $(COMPILER)/sdasz80

OUTDIR = .$(TARGET)

ifeq ($(MAKELEVEL),0)
	dummy := $(shell echo -e "Make gb")
	dummy := $(shell mkdir -p $(OUTDIR))
endif

#SRC = gb.c
SRC =	xprintf.c \
		isr.c \
		dma_test.c
# isr_test.c
# beep.c

ASRC = vectors.asm

OBJ = $(addprefix $(OUTDIR)/,$(SRC:.c=.rel) $(ASRC:.asm=.rel))

CFLAGS = -msm83\

LDFLAGS = \

.PHONY: all compile bin clean

all: compile bin

$(OUTDIR)/%.rel : %.c
	@echo "compiling $< ..."
	$(CC) $(CFLAGS)  -c $< -o $@

$(OUTDIR)/%.rel : %.asm
	@echo "compiling $< ..."
	$(ASM) -o $@ $<
	$(ASM) -l $(@:.rel=.lst) $<


$(OUTDIR)/$(TARGET).ihx: $(OBJ)
	$(CC) -msm83 --code-loc 0x200 --data-loc 0xC000 -I"C:/Program Files (x86)/SDCC/lib/sm83" -o $(OUTDIR)/$(TARGET).ihx $(OBJ)

bin: compile $(OUTDIR)/gb.bin

$(OUTDIR)/gb.bin: $(OUTDIR)/gb.ihx
	@echo "generating $@ ..."
	$(MAKEBIN) $< $@

clean:
	rm -rf $(OUTDIR)
