
prefix=/usr
GCC=gcc
STRIP=strip

NAME=photorecover

all:
	$(GCC) -o $(NAME) main.c

install:
	$(STRIP) $(NAME)
	cp $(NAME) $(DESTDIR)/$(prefix)/bin
	cp man/$(NAME).1 $(DESTDIR)/$(prefix)/share/man/man1

clean:
	rm $(NAME) *.o core

