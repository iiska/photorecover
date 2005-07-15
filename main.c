
#include <stdio.h>
#include <stdlib.h>

// Bytes
#define DEFAULTBLOCKSIZE 512
#define ROOTSIZE 32

/* Suoraan levyltä luettu 3 tavun sarja, jotka täytyy vielä
 * muuttaa fat12 käyttämään 2 * 12bit muotoon */
typedef struct {
	unsigned char a; // 0x(a2)(a3)
	unsigned char ba;// 0x(b3)(a1)
	unsigned char b; // 0x(b1)(b2)
} tmp_fat12_data;

FILE *image;

int blocks; // devices block count
int blocksize = DEFAULTBLOCKSIZE; // bytes per sector
int clustersize = 1; // sectors per cluster
int fatnumber = 1; // number of FATs
int fatsize = 1; // sectors per FAT
int rootcount = 1; // rootdir entry count
int datastart = 0; // data area start block

int reservedblocks = 1;




long int photo_start = 0;
long int photo_end = 0;

void locate_photo();
void save_photo(long int new_start);
unsigned short int get_12bit_entry(char offset, tmp_fat12_data data);

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

	/* Määritetään sektorit per klusteri, 13. tavu */
	clustersize = fgetc(image);

	/* Varattujen sektoreiden määrä alussa 14-15 tavut */
	reservedblocks = fgetc(image) + (fgetc(image) << 8);
	printf("Reserved: %d\n",reservedblocks);

	/* FATtien määrä 16. tavu */
	fatnumber = fgetc(image);
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
		printf("Cluster: %X\n", i/clustersize);
		fseek(image,i*blocksize,SEEK_SET);
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
void locate_photo() {
	
	if ( (fgetc(image) << 8) + fgetc(image) == 0xFFD8 ) {
		if (photo_start == 0) {
			photo_start = ftell(image)-2;	
			printf("Cluster %d: Jpeg-file start\n",
				(photo_start) / blocksize / clustersize);
		} else {
			photo_end = ftell(image)-3;
			printf("Cluster %d: Jpeg-file start\n",
				(photo_end+1) / blocksize / clustersize);
			save_photo(photo_end+1);
			
		}
	}

	fseek(image, -2, SEEK_CUR);
	int i;
	for(i=0;i<blocksize;i++) {
		if ( (fgetc(image) + (fgetc(image) << 8)) == 0xFFD9 ) {
			if (photo_start != 0) {
				photo_end = ftell(image);
		  		printf("Cluster %d (Byte %d): Jpeg-file end\n",
					(photo_end-i)/blocksize/clustersize, i);
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

        fseek(image,photo_start,SEEK_SET);	
	fread(&data,sizeof(char),count,image);
	fseek(image,-(count * sizeof(char)),SEEK_CUR);

	if (new_start == 0)
		tmp = "cluster_";
	else
		tmp = "damaged_";

	filename = (char *)malloc(s);
	n = snprintf(filename,s,"%s%d.jpg",tmp,(photo_start/blocksize));

	if (n >= s) {
		filename = (char *)realloc(filename,n-s+1);
		sprintf(filename,"%s%d.jpg",tmp,(photo_start/blocksize));
	}

	FILE *output = fopen(filename,"w");
	fwrite(&data,sizeof(char),count,output);
	fclose(output);

	photo_start = new_start;
	photo_end = 0;
}

/* offset = 0 tai 1, eli otetaanko ensimmäinen vai toinen 12bit pala
 * tiedostossa: uv wx yz -> 12 bittisenä -> xuv yzw */
unsigned short int get_12bit_entry(char offset, tmp_fat12_data data) {
	if (offset == 0) {
		/* ba & 0x0F -> ba AND 00001111, eli nollaa 4 ensimmäistä
		 * bittiä */
		return ( ((data.ba & 0x0F) << 8) + data.a);
	} else if (offset == 1) {
		return ( ((data.ba & 0xF0) >> 4) + (data.b << 4) );
	}
	else {
		return 0;
	}
}
