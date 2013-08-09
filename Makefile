
OPTIMIZE := -O2

all: jshello jsxhello

jshello: nore
	./nore hello.js foo bar

jsxhello: nore
	JSX_RUNJS=./nore jsx --run hello.jsx foo bar

nore: main.c lib/libuv.a
	$(CC) $(OPTIMIZE) -Iinclude -Wall -Wextra -g -framework JavaScriptCore -framework CoreFoundation -framework CoreServices -o $@ $< lib/libuv.a

lib/libuv.a:
	prefix=$$PWD ; cd libuv && sh autogen.sh && ./configure --prefix=$$prefix && make install
