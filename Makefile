CFLAGS = -Wall -Wextra -std=c89 -pedantic -Wwrite-strings

all: ckdu

ckdu: ckdu.o

clean:
	rm -f *.o
