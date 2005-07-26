
prefix=/usr/local
GCC=gcc
STRIP=strip

NAME=photorecover

all:
	$(GCC) -o $(NAME) main.c

install:
	$(STRIP) $(NAME)
	cp $(NAME) $(prefix)/bin
	cp man/$(NAME).1.gz $(prefix)/share/man/man1

clean:
	rm $(NAME) *.o core > /dev/null 2> /dev/null

