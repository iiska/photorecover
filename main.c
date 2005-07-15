
#include <stdio.h>
#include <stdlib.h>

#define BLOCKSIZE 512

FILE *image;

int blocks; // devices block count

long int photo_start = 0;
long int photo_end = 0;

void locate_photo();
void save_photo(long int new_start);

int main(int argc, char *argv[]) {
	if (argc == 2) {
		image = fopen(argv[1],"r");
	} else {
		printf("\nUsage: %s [flash image]\n\n", argv[0]);
		return 1;
	}
	fseek(image,0,SEEK_END); // Siirrytään tiedoston loppuun
	blocks = ftell(image) / BLOCKSIZE;
	if (ftell(image)%BLOCKSIZE != 0) {
		printf("Wrong sector count! Sectors \% blocksize != 0\n");
	}
	int i;
	for (i=0; i<blocks; i++) {
		fseek(image,i*BLOCKSIZE,SEEK_SET);
		locate_photo();
	}
	
	fclose(image);
	return 0;
}

/* Etsii valokuvan ja tallentaa sen alun ja lopun sijainnin photo_start ja -end
 * -muuttujiin.
 *  
 * Jos sektori alkaa tavuilla 0xFFD8 siinä on todennäköisesti jpeg-tiedoston
 * alku.
 * Vastaavasti 0xFFD9 ilmoittaa tiedoston lopun.
 */
void locate_photo() {;
	if ( (fgetc(image) == 0xFF) && (fgetc(image) == 0xD8)) {
		if (photo_start == 0) {
			photo_start = ftell(image)-2;	
			printf("Cluster %d: Jpeg-file start\n",
				(photo_start) / BLOCKSIZE);
		} else {
			photo_end = ftell(image)-3;
			printf("Cluster %d: Jpeg-file start\n",
				(photo_end+1) / BLOCKSIZE);
			save_photo(photo_end+1);
			
		}
	}

	fseek(image, -2, SEEK_CUR);
	int i;
	for(i=0;i<BLOCKSIZE;i++) {
		if ( (fgetc(image) == 0xFF) && (fgetc(image) == 0xD9)) {
			if (photo_start != 0) {
				photo_end = ftell(image);
		  		printf("Cluster %d (Byte %d): Jpeg-file end\n",
					(photo_end-i)/BLOCKSIZE, i);
				save_photo(0);
			}
		}	
	}
}

void save_photo(long int new_start) {
	char *filename, *tmp;
	short int s = 16, n;
	
	long int count = photo_end-photo_start;
	unsigned char data[count+1];
	
	fread(&data,sizeof(char),count,image);
	fseek(image,-(count * sizeof(char)),SEEK_CUR);

	if (new_start == 0)
		tmp = "cluster_";
	else
		tmp = "damaged_";

	filename = (char *)malloc(s);
	n = snprintf(filename,s,"%s%d.jpg",tmp,(photo_start/BLOCKSIZE));

	if (n >= s) {
		filename = (char *)realloc(filename,n-s+1);
		sprintf(filename,"%s%d.jpg",tmp,(photo_start/BLOCKSIZE));
	}

	FILE *output = fopen(filename,"w");
	fwrite(&data,sizeof(char),count,output);
	fclose(output);

	photo_start = new_start;
	photo_end = 0;
}
