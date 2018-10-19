##
# Vita PlayBack client
#
# @file
# @version 0.1

LIB = ../lib
PROJECT = plybk-vita
PROJECT_TITLE = PlayBack Vita
PROJECT_TITLEID = PLYBCKVIT


SRC=${wildcard *.c} $(wildcard ${LIB}/*.c )
OBJ=${SRC:.c=.vita.o}
DEP=${SRC:.c=.d}
INC=${LIB}

CFLAGS=-std=gnu99 -Wall -Wextra -O2 -Wl,-q -flto
LDFLAGS= -Wl,-q

CC := arm-vita-eabi-gcc
STRIP := arm-vita-eabi-strip
LIBS = -lSceDisplay_stub -lSceSysmodule_stub

.PHONY: all clean clang-complete

all:  clang-complete package

package: $(PROJECT).vpk

$(PROJECT).vpk: eboot.bin param.sfo
	vita-pack-vpk -s param.sfo -b eboot.bin \
		--add sce_sys/icon0.png=sce_sys/icon0.png \
		--add sce_sys/livearea/contents/bg.png=sce_sys/livearea/contents/bg.png \
		--add sce_sys/livearea/contents/startup.png=sce_sys/livearea/contents/startup.png \
		--add sce_sys/livearea/contents/template.xml=sce_sys/livearea/contents/template.xml \
	$(PROJECT).vpk

eboot.bin: $(PROJECT).velf
	vita-make-fself -c $(PROJECT).velf eboot.bin

param.sfo:
	vita-mksfoex -s TITLE_ID="$(PROJECT_TITLEID)" "$(PROJECT_TITLE)" param.sfo

$(PROJECT).velf: $(PROJECT).elf
	$(STRIP) -g $<
	vita-elf-create $< $@

clean:
	@${RM} ${BIN} ${OBJ} ${DEP}

$(PROJECT).elf: ${OBJ}
	@echo [L] $@
	${CC} ${LDFLAGS} $^ $(LIBS) -o $@

%.vita.o: %.c
	@echo [C] $<
	${CC} ${CFLAGS} -MMD -I${INC} -c -o $@ $<

clang-complete:
	echo "-I$(LIB)" >.clang_complete

-include ${DEP}

# end
