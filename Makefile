
OPTIMIZE := -O4

all: jshello jsxhello

jshello: nore
	./nore hello.js foo bar

jsxhello: nore
	JSX_RUNJS=./nore jsx --run hello.jsx foo bar

nore: main.c
	$(CC) $(OPTIMIZE) -Wall -Wextra -g -framework JavaScriptCore -o $@ $<

