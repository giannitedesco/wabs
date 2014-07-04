.SUFFIXES:

CC := $(CROSS_COMPILE)gcc
LD := $(CROSS_COMPILE)ld
AR := $(CROSS_COMPILE)ar

EXTRA_DEFS := -D_FILE_OFFSET_BITS=64
CFLAGS := -g -pipe -O2 -Wall \
	-flto -fwhole-program -mtune=corei7 \
	-Wsign-compare -Wcast-align \
	-Wstrict-prototypes \
	-Wmissing-prototypes \
	-Wmissing-declarations \
	-Wmissing-noreturn \
	-finline-functions \
	-Wmissing-format-attribute \
	-Wno-cast-align \
	-fwrapv \
	-Iinclude \
	$(EXTRA_DEFS) 

WORMS_BIN := worms
WORMS_LIBS := -lreadline
WORMS_OBJ := worms.o \
		wa.o \
		hexdump.o

ALL_BIN := $(WORMS_BIN)
ALL_OBJ := $(WORMS_OBJ)
ALL_DEP := $(patsubst %.o, .%.d, $(ALL_OBJ))
ALL_TARGETS := $(ALL_BIN)

TARGET: all

.PHONY: all clean

all: $(ALL_BIN)

ifeq ($(filter clean, $(MAKECMDGOALS)),clean)
CLEAN_DEP := clean
else
CLEAN_DEP :=
endif

%.o %.d: %.c $(CLEAN_DEP) $(ROOT_DEP) $(CONFIG_MAK) Makefile
	@echo " [C] $<"
	@$(CC) $(CFLAGS) -MMD -MF $(patsubst %.o, .%.d, $@) \
		-MT $(patsubst .%.d, %.o, $@) \
		-c -o $(patsubst .%.d, %.o, $@) $<

$(WORMS_BIN): $(WORMS_OBJ)
	@echo " [LINK] $@"
	@$(CC) $(CFLAGS) -o $@ $(WORMS_OBJ) $(WORMS_LIBS)

clean:
	rm -f $(ALL_TARGETS) $(GRAMMAR) $(ALL_OBJ) $(ALL_DEP) tagops.c lex.h

ifneq ($(MAKECMDGOALS),clean)
-include $(ALL_DEP)
endif
