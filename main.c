
#include <stdio.h>
#include <stdlib.h>

FILE *image;

int main(int argc, char *argv[]) {
	if (argc == 2) {
		image = fopen(argv[1],"r");
	} else {
		printf("\nUsage: %s [flash image]\n\n", argv[0]);
		return 1;
	}
	
	return 0;
}
