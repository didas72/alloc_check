CC=gcc
C_FLAGS=-O3 -Wall -Wextra -Wno-unused-result -g
VAL_FLAGS=--leak-check=full

SRC=src
TEST=tests
OBJ=build/obj
BIN=build/bin

OUTBIN=$(BIN)/main
TESTBIN=$(BIN)/tester

SRCS=$(wildcard $(SRC)/*.c)
OBJS=$(patsubst $(SRC)/%.c, $(OBJ)/%.o, $(SRCS))


.PHONY: all run build debug memleak clean loc


all: build
build: $(OUTBIN)



run: $(OUTBIN)
	$(OUTBIN)


$(OUTBIN): $(OBJS)
	@mkdir -p $(@D)
	$(CC) $(C_FLAGS) $(OBJS) -o $@

$(OBJ)/%.o: $(SRC)/%.c
	@mkdir -p $(@D)
	$(CC) $(C_FLAGS) -c $< -o $@



debug: $(TESTBIN)
	gdb ./$(TESTBIN)

memleak: $(TESTBIN)
	valgrind $(VAL_FLAGS) $(TESTBIN)


clean:
	$(RM) -r $(OBJ) $(BIN)

loc:
	scc -s lines --no-cocomo --no-gitignore -w --size-unit binary --exclude-ext md,makefile,json --exclude-dir tests/framework
