.PHONY: all clean

all: shell

shell: shell.c
	gcc -o shell shell.c

clean: 
	rm -f shell