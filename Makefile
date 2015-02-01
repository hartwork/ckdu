CFLAGS += -Wall -Wextra -std=c89 -pedantic -Wwrite-strings

all: ckdu

clean:
	$(RM) ckdu

.PHONY: all clean
