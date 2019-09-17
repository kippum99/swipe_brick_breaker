CC = clang
CFLAGS = -Iinclude -Wall -g -fno-omit-frame-pointer -fsanitize=address -O0
LIB_MATH = -lm
LIBS = $(LIB_MATH) -lSDL2 -lSDL2_gfx -lSDL2_ttf

# List of C files in "libraries"
CUSTOM_LIBS = sdl_wrapper vector list polygon body scene collision forces
OBJS = $(addprefix out/,$(CUSTOM_LIBS:=.o))

GAME = game

all: $(addprefix bin/,$(GAME))

out/%.o: library/%.c # source file may be found in "library"
	$(CC) -c $(CFLAGS) $^ -o $@
out/game.o: game.c
	$(CC) -c $(CFLAGS) $^ -o $@

bin/%: out/game.o $(OBJS)
	$(CC) $(CFLAGS) $(LIBS) $^ -o $@

clean:
	rm -f out/* bin/*

.PHONY: all clean
.PRECIOUS: out/%.o
