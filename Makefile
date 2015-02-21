GCC = gcc
CFLAGS += -c -Wall
#CFLAGS += -g -DDEBUG

BIN = ifdump
SRC = ifdump.c
OBJ = $(SRC:.c=.o)

.PHONY: all clean
all: $(SRC) $(BIN)

$(BIN): $(OBJ)
	$(GCC) $(OBJ) -o $@ $(LDFLAGS)

.cc.o:
	$(GCC) $(CFLAGS) $< -o $@

clean:
	rm -f $(BIN) $(OBJ)
