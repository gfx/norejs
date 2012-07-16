
OPTIMIZE := -O4

hello: nore
	./nore hello.js

nore: main.c
	$(CC) $(OPTIMIZE) -Wall -Wextra -g -framework JavaScriptCore -o $@ $<

