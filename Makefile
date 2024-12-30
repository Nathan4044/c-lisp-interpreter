build:
	@cc *.c -o lisp
debug-build:
	@cc *.c -g -o lisp
debug: debug-build
	@lldb lisp
run: build
	@rlwrap ./lisp
