
prefix=/usr/local
GCC=gcc
STRIP=strip

NAME=photorecover

all:
	$(GCC) -o $(NAME) main.c

install:
	$(STRIP) $(NAME)
	cp $(NAME) $(prefix)/bin

clean:
	rm $(NAME) *.o core

