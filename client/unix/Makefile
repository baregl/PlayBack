##
# Unix PlayBack client
#
# @file
# @version 0.1

LIB = ../lib
BIN = plybk_c

SRC=${wildcard *.c} $(wildcard ${LIB}/*.c )
OBJ=${SRC:.c=.o}
DEP=${SRC:.c=.d}
INC=${LIB}

CFLAGS=-std=gnu99 -Wall -Wextra -O2 -flto
LDFLAGS=-flto

.PHONY: all clean

all:  clang-complete ${BIN}

clean:
	@${RM} ${BIN} ${OBJ} ${DEP}

${BIN}: ${OBJ}
	@echo [L] $@
	${CC} ${LDFLAGS} -o $@ $^

%.o: %.c
	@echo [C] $<
	${CC} ${CFLAGS} -MMD -I${INC} -c -o $@ $<

clang-complete:
	echo "-I$(LIB)" >.clang_complete

-include ${DEP}

# end
