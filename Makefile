build:
	@cc *.c -Wall -Werror -o lisp
debug-build:
	@cc *.c -Wall -Werror -g -o lisp
debug: debug-build
	@lldb lisp
run: build
	@rlwrap ./lisp
