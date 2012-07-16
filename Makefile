
OPTIMIZE := -O4

all: jshello jsxhello

jshello: nore
	./nore hello.js foo bar

jsxhello: nore
	jsx --output hello.jsx.js --executable node hello.jsx
	./nore hello.jsx.js foo bar

nore: main.c
	$(CC) $(OPTIMIZE) -Wall -Wextra -g -framework JavaScriptCore -o $@ $<

