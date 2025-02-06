P=lisp
OBJECTS = chunk.o compiler.o debug.o memory.o nativeFns.o object.o scanner.o table.o value.o vm.o
CFLAGS = -g -pg -Wall -Werror -O3
CC=clang -std=c99

$(P): $(OBJECTS)

clean:
	@rm -f $(OBJECTS)
