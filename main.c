
#include <stdio.h>
#include <stdlib.h>

// Bytes
#define DBLOCKSIZE 512
#define ROOTSIZE 32

#define DCLUSTERSIZE 1
#define DRESERVED 1
#define DFATN 2


FILE *image;

int blocks; // devices block count
int blocksize; // bytes per sector
int clustersize; // sectors per cluster
int fatnumber; // number of FATs
int fatsize; // sectors per FAT
int rootcount; // rootdir entry count
int datastart; // data area start block

int reservedblocks;




long int photo_start = 0;
long int photo_end = 0;

void locate_photo();
void save_photo(long int new_start);
int is_thumbnail();
void * xmalloc(size_t size);
void * xrealloc(void *ptr,size_t size);

int main(int argc, char *argv[]) {
	if (argc == 2) {
		image = fopen(argv[1],"r");
	} else {
		printf("\nUsage: %s [flash image]\n\n", argv[0]);
		return 1;
	}
	/* Määritetään tavut per sektori, 11-12 tavut */
	fseek(image,11,SEEK_SET);
	blocksize = fgetc(image) + (fgetc(image) << 8);
	if ( (blocksize % DBLOCKSIZE != 0) || (blocksize == 0) ) {
		blocksize = DBLOCKSIZE;
		printf("Couldn't find blocksize from bootsector. Using default %d\n",
				DBLOCKSIZE);
	} else {
		printf("Blocksize: %d\n",blocksize);
	}

	/* Määritetään sektorit per klusteri, 13. tavu */
	clustersize = fgetc(image);
	if (clustersize == 0)
		clustersize = DCLUSTERSIZE;
	printf("Clustersize: %d\n",clustersize);

	/* Varattujen sektoreiden määrä alussa 14-15 tavut */
	reservedblocks = fgetc(image) + (fgetc(image) << 8);
	if (reservedblocks == 0)
		reservedblocks = DRESERVED;
	printf("Reserved: %d\n",reservedblocks);

	/* FATtien määrä 16. tavu */
	fatnumber = fgetc(image);
	if (fatnumber == 0)
		fatnumber = DFATN;
	printf("Fats: %d\n",fatnumber);

	/* Kansiolistausten määrä 17-18, tavut */
	rootcount = fgetc(image) + (fgetc(image) << 8);
	printf("Roots: %d\n", rootcount);

	/* Sektoreita per FAT, tavut 22-23 */
	fseek(image,22,SEEK_SET);
	fatsize = fgetc(image) + (fgetc(image) << 8);
	printf("Fatsize: %d\n",fatsize);

	datastart = reservedblocks + (fatnumber * fatsize) + (((rootcount * ROOTSIZE) % blocksize + (rootcount * ROOTSIZE)) / blocksize);
	printf("Datastart: %d\n",datastart);
	
	
	fseek(image,0,SEEK_END); // Siirrytään tiedoston loppuun
	blocks = ftell(image) / blocksize;
	if (ftell(image)%blocksize != 0) {
		printf("Wrong sector count! Sectors \% blocksize != 0\n");
	}
	
	int i;
	for (i=datastart; i<blocks; i+=clustersize) {
		fseek(image,i*blocksize,SEEK_SET);
		locate_photo(i*blocksize);
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
void locate_photo(int stream_start) {
	
	if ( (fgetc(image) << 8) + fgetc(image) == 0xFFD8 ) {
		printf("Cluster %d: Jpeg-file start\n",
				(stream_start) / blocksize / clustersize);
		if (photo_start == 0) {
			if (is_thumbnail() == 1)
			  photo_start = stream_start;

		} else {
			photo_end = stream_start-1;
			if (is_thumbnail() == 1)
				save_photo(stream_start);
			else
				save_photo(0);
			
		}
	}

	fseek(image, -2, SEEK_CUR);
	if (photo_start != 0) {
		int i;
		for(i=0;i<(blocksize*clustersize);i++) {
			if ( (fgetc(image) << 8 ) + fgetc(image) == 0xFFD9 ) {
				photo_end = ftell(image);
		  		printf("Cluster %d (Byte %d): Jpeg-file end\n",
					(photo_end-i)/blocksize/clustersize, i);
				save_photo(0);
				break;
			}	
		}
	}
}

void save_photo(long int new_start) {
	char *filename, *tmp;
	short int s = 16, n;
	
	long int count = photo_end-photo_start;
	unsigned char data[count+1];

        fseek(image,photo_start,SEEK_SET);	
	fread(&data,sizeof(char),count,image);
	fseek(image,-(count * sizeof(char)),SEEK_CUR);

	if (new_start == 0)
		tmp = "cluster_";
	else
		tmp = "damaged_";

	filename = (char *)xmalloc(s);
	n = snprintf(filename,s,"%s%d.jpg",tmp,(photo_start/blocksize));

	if (n >= s) {
		filename = (char *)xrealloc(filename,n-s+1);
		sprintf(filename,"%s%d.jpg",tmp,(photo_start/blocksize));
	}

	FILE *output = fopen(filename,"w");
	free(filename);
	fwrite(&data,sizeof(char),count,output);
	fclose(output);

	photo_start = new_start;
	photo_end = 0;
}

/* Checks jpeg-dimensions found in current stream position
 * seeks to the same position in the end and returns 1 if thumbnail
 * 0 otherwise
 */
int is_thumbnail() {
	int apmarker = (fgetc(image) << 8) + fgetc(image);
	int c;
	switch(apmarker) {
		case 0xFFE0:
			printf("Jpeg/JFIF image\n");
			break;
		case 0xFFE1:
			printf("Jpeg/Exif image\n");
			break;
	}
	do {
	do {
		c = fgetc(image);
		if (c == EOF) {
			printf("ERROR: Unexpected end of file!\n");
			exit(1);
		}
	} while(c != 0xFF);
	do {
		c = getc (image);
		if (c == EOF) {
			printf("ERROR: Unexpected end of file!\n");
			exit(1);
		}
	} while (c == 0xff);
	if ((c & 0xf0) == 0xc0 && c != 0xc4 && c != 0xcc) {
		if ( (fgetc(image) << 8) + fgetc(image) < 2 ) {
			printf("ERROR: erroneous JPEG marker length\n");
			return 1;
		}
		fseek(image,1,SEEK_CUR); // skip precision
		int h = (fgetc(image) << 8) + fgetc(image);
		int w = (fgetc(image) << 8) + fgetc(image);
		if ( (h < 480) || (w < 640) ) {
			printf("Thumbnail skipped\n");
			return 0;
		} else {
			printf("Jpeg dimensions: %dx%d\n",w,h);
			return 1;
		}
	}
	} while(1);
}

void * xmalloc(size_t size) {
	register void *val = malloc(size);
	if (val == 0) {
		printf("ERROR: not enough virtual memory!\n");
		exit(1);
	}
	return val;
}

void * xrealloc(void *ptr, size_t size) {
	register void *val = realloc(ptr,size);
	if (val == 0) {
		printf("ERROR: not enough virtual memory!\n");
		exit(1);
	}
	return val;
}
