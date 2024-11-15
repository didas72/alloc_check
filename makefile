AR=ar
AR_FLAGS=rcs
CC=gcc
C_FLAGS=-O2 -Wall -Wextra -Wno-unused-result

DIR_SRC=src
DIR_INC=include
DIR_BUILD=build

OUTBIN=$(DIR_BUILD)/bin/liballoc_check.a

SRCS=$(wildcard $(DIR_SRC)/*.c)
OBJS=$(patsubst $(DIR_SRC)/%.c, $(DIR_BUILD)/obj/%.o, $(SRCS))

.PHONY: all build clean loc

all: build
build: $(OUTBIN)

$(OUTBIN): $(OBJS)
	@mkdir -p $(@D)
	$(AR) $(AR_FLAGS) $@ $^

$(DIR_BUILD)/obj/%.o: $(DIR_SRC)/%.c
	@mkdir -p $(@D)
	$(CC) $(C_FLAGS) -I$(DIR_INC) -c $< -o $@

clean:
	$(RM) -r $(DIR_OBJ) $(DIR_BUILD)

loc:
	scc -s lines --no-cocomo --no-gitignore -w --size-unit binary --exclude-ext md,makefiles
