P=lisp
OBJECTS = chunk.o compiler.o debug.o memory.o nativeFns.o object.o scanner.o table.o value.o vm.o
CFLAGS = -lm -g -pg -Wall -Werror -Wextra -Wpedantic -Wconversion -Wdeprecated -Wno-pedantic -O3
CC=cc

$(P): $(OBJECTS)

clean:
	@rm -rf $(OBJECTS) $(P).dSYM
