GCC=gcc

NAME=photorecover

all:
	$(GCC) -o $(NAME) main.c

install:

clean:
	rm $(NAME) *.o core

