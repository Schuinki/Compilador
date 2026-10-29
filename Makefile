CC     = gcc
CFLAGS = -std=c11 -Wall -Wextra -Wpedantic
SRC    = lexer.c grammar.c lalr.c parser.c scope.c semantics.c codegen.c vm.c
HDR    = lexer.h grammar.h lalr.h parser.h scope.h semantics.h codegen.h vm.h

all: ssl testes

ssl: main.c $(SRC) $(HDR)
	$(CC) $(CFLAGS) -o ssl main.c $(SRC)

testes: tests.c $(SRC) $(HDR)
	$(CC) $(CFLAGS) -o testes tests.c $(SRC)

test: all
	./testes

clean:
	rm -f ssl testes

.PHONY: all test clean
