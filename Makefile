CFLAGS=-Wall

all: 
	$(CC) $(CFLAGS) -o svn-shell src/shell.c

clean:
	rm -f svn-shell

debug:
	$(CC) -g -o svn-shell-debug src/shell_debug.c

test: clean debug
	$(shell ./tests/test $<)

install:
	strip svn-shell
	install -m 755 -D -t /bin/ svn-shell
