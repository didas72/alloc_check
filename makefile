AR=ar
AR_FLAGS=rcs
CC=gcc
C_FLAGS=-O2 -Wall -Wextra -Wno-unused-result -DSUS_TARGET_VERSION=10000

DIR_SRC=src
DIR_INC=include
DIR_BUILD=build

LIB_NAME=liballoc_check.a

OUTBIN=$(DIR_BUILD)/bin/$(LIB_NAME)
PREFIX=/usr/local

SRCS=$(wildcard $(DIR_SRC)/*.c)
OBJS=$(patsubst $(DIR_SRC)/%.c, $(DIR_BUILD)/obj/%.o, $(SRCS))

.PHONY: all build rebuild install uninstall clean loc

all: build
build: $(OUTBIN)
rebuild: clean build

$(OUTBIN): $(OBJS)
	@mkdir -p $(@D)
	$(AR) $(AR_FLAGS) $@ $^

$(DIR_BUILD)/obj/%.o: $(DIR_SRC)/%.c
	@mkdir -p $(@D)
	$(CC) $(C_FLAGS) -I$(DIR_INC) -c $< -o $@

install: $(OUTBIN)
	mkdir -p $(PREFIX)/lib
	cp $(OUTBIN) $(PREFIX)/lib/
	mkdir -p $(PREFIX)/include/alloc_check
	cp $(DIR_INC)/*.h $(PREFIX)/include/alloc_check/

uninstall:
	rm -f $(PREFIX)/lib/$(LIB_NAME)
	rm -rf $(PREFIX)/include/alloc_check

clean:
	$(RM) -r $(DIR_BUILD)

loc:
	scc -s lines --no-cocomo --no-gitignore -w --size-unit binary --exclude-ext md,makefiles
