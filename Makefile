build:
	@cc *.c -o lisp
run: build
	@rlwrap ./lisp
