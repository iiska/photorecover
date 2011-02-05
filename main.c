/*
 * Copyright 2005 Juhamatti Niemelä
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#define VERSION "0.9.0"

#include <stdio.h>
#include <stdlib.h>

// Bytes
#define DBLOCKSIZE 512
#define ROOTSIZE 32

#define DCLUSTERSIZE 1
#define DRESERVED 1
#define DFATN 2

// Files smaller than this are considered as thumbnails
#define TRESH_KB 150


FILE *image;

int blocks; // devices block count
int blocksize; // bytes per sector
int clustersize; // sectors per cluster
int fatnumber; // number of FATs
int fatsize; // sectors per FAT
int rootcount; // rootdir entry count
int datastart; // data area start block

int reservedblocks;




unsigned long int photo_start = 0;
unsigned long int photo_end = 0;
unsigned long int exif_end = 0;

void locate_photo();
void save_photo(long int new_start);
void * xmalloc(size_t size);
void * xrealloc(void *ptr,size_t size);
unsigned int read_byte(FILE *stream);
unsigned int read_msb_word(FILE *stream);
unsigned int read_lsb_word(FILE *stream);
void error(char *msg);

void usage();

int main(int argc, char *argv[]) {
	if (argc == 2) {
		image = fopen(argv[1],"r");
	} else {
		usage(argv[0]);
		return 1;
	}
	/* Määritetään tavut per sektori, 11-12 tavut */
	fseek(image,11,SEEK_SET);
	blocksize = read_lsb_word(image);
	if ( (blocksize % DBLOCKSIZE != 0) || (blocksize == 0) ) {
		blocksize = DBLOCKSIZE;
		printf("Couldn't find blocksize from bootsector. Using default %d\n",
				DBLOCKSIZE);
	} else {
		printf("Blocksize: %d\n",blocksize);
	}

	/* Määritetään sektorit per klusteri, 13. tavu */
	clustersize = read_byte(image);
	if (clustersize == 0)
		clustersize = DCLUSTERSIZE;
	printf("Clustersize: %d\n",clustersize);

	/* Varattujen sektoreiden määrä alussa 14-15 tavut */
	reservedblocks = read_lsb_word(image);
	if (reservedblocks == 0)
		reservedblocks = DRESERVED;
	printf("Reserved: %d\n",reservedblocks);

	/* FATtien määrä 16. tavu */
	fatnumber = read_byte(image);
	if (fatnumber == 0)
		fatnumber = DFATN;
	printf("Fats: %d\n",fatnumber);

	/* Kansiolistausten määrä 17-18, tavut */
	rootcount = read_lsb_word(image);
	printf("Roots: %d\n", rootcount);

	/* Sektoreita per FAT, tavut 22-23 */
	fseek(image,22,SEEK_SET);
	fatsize = read_lsb_word(image);
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
		if (exif_end < ((i+1)*blocksize) ) {
			fseek(image,i*blocksize,SEEK_SET);
			locate_photo(i*blocksize);
		}
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
void locate_photo(unsigned long int stream_start) {

	if ( read_msb_word(image) == 0xFFD8 ) {
		printf("Cluster %d: Jpeg-file start\n",
				(stream_start) / blocksize / clustersize);

		if (photo_start == 0) {
			  photo_start = stream_start;
		} else {
			photo_end = stream_start-1;
			save_photo(stream_start);
		}
	}

	if ( (photo_start != 0) ) {
		fseek(image,stream_start,SEEK_SET);
		int i = 0;
		do {
			if (read_byte(image) == 0xFF) {
				int c = read_byte(image);
				i++;
				if ( c == 0xD9) {
					photo_end = ftell(image);
		  			printf("Cluster %d (Byte %d): Jpeg-file end\n",
					(photo_end-i)/blocksize/clustersize, i);
					save_photo(0);
					return;
				} else if (c == 0xE1) {
					// i+1, koska ensimmäisen if-lauseen
					// luku huomioidaan vasta silmukan
					// lopussa
					exif_end = stream_start + (i+1) +
						read_msb_word(image) - 2;
					printf("Found exif-header. Exif-end at %ld\n", exif_end);
					return;
				}
			}
			i++;
		} while (i < (blocksize * clustersize));
	}
}

void save_photo(long int new_start) {
	char *filename, *tmp;
	short int s = 16, n;

	long int count = photo_end-photo_start;
	if (count < (TRESH_KB * 1024)) {
		printf("File in clusters %d-%d is thumbnail. Skipping.\n",
				photo_start/blocksize/clustersize,
				photo_end/blocksize/clustersize);
		photo_start = new_start;
		return;
	}
	unsigned char *data;
	data = malloc(count+1);

        fseek(image,photo_start,SEEK_SET);
	fread(data,sizeof(char),count,image);
	fseek(image,-(count * sizeof(char)),SEEK_CUR);

	if (new_start == 0)
		tmp = "cluster_";
	else
		tmp = "damaged_";

	filename = (char *)xmalloc(s);
	n = snprintf(filename,s,"%s%d.jpg",tmp,(photo_start/blocksize/clustersize));

	if (n >= s) {
		filename = (char *)xrealloc(filename,n-s+1);
		sprintf(filename,"%s%d.jpg",tmp,(photo_start/blocksize/clustersize));
	}

	FILE *output = fopen(filename,"w");
	free(filename);
	fwrite(data,sizeof(char),count,output);
	fclose(output);
        // TODO: Implement way to keep allocated memory for data
        // around for multiple iterations to improve performance
	free(data);

	photo_start = new_start;
	photo_end = 0;
}


void * xmalloc(size_t size) {
	register void *val = malloc(size);
	if (val == 0)
		error("not enough virtual memory!");

	return val;
}

void * xrealloc(void *ptr, size_t size) {
	register void *val = realloc(ptr,size);
	if (val == 0)
		error("not enough virtual memory!");

	return val;
}

unsigned int read_byte(FILE *stream) {
	unsigned int c = fgetc(stream);

	if (c == EOF)
		error("premature end of file");

	return c;
}

unsigned int read_msb_word(FILE *stream) {
	unsigned int c = (fgetc(stream) << 8) + fgetc(stream);

	if (c == EOF)
		error("premature end of file");

	return c;
}

unsigned int read_lsb_word(FILE *stream) {
	unsigned int c = fgetc(stream) + (fgetc(stream) << 8);

	if (c == EOF)
		error("premature end of file");

	return c;
}


void error(char *msg) {
	printf("ERROR: %s\n",msg);
	exit(1);
}

void usage(char* cmd) {
	printf("Photorecover version %s\nUtility to recover erased Jpeg/Exif/Jfif files from a memory card\n\nUsage:\n\t%s [flash image | device file]\n\nReport bugs to <iiska@ee.oulu.fi>\n", VERSION, cmd);
}
