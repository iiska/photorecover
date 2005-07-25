
prefix=/usr
GCC=gcc
STRIP=strip

NAME=photorecover

all:
	$(GCC) -o $(NAME) main.c

install:
	$(STRIP) $(NAME)
	cp $(NAME) $(DESTDIR)/$(prefix)/bin

clean:
	rm $(NAME) *.o core

