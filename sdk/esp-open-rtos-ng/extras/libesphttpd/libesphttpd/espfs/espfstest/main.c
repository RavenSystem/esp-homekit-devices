/*
Simple and stupid file decompressor for an espfs image. Mostly used as a testbed for espfs.c and 
the decompressors: code compiled natively is way easier to debug using gdb et all :)
*/
#include <stdio.h>
#include <stdint.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>


#include "espfs.h"

char *espFsData;

int main(int argc, char **argv) {
	int f, out;
	int len;
	char buff[128];
	EspFsFile *ef;
	off_t size;
	EspFsInitResult ir;

	if (argc!=3) {
		printf("Usage: %s espfs-image file\nExpands file from the espfs-image archive.\n", argv[0]);
		exit(0);
	}

	f=open(argv[1], O_RDONLY);
	if (f<=0) {
		perror(argv[1]);
		exit(1);
	}
	size=lseek(f, 0, SEEK_END);
	espFsData=mmap(NULL, size, PROT_READ, MAP_SHARED, f, 0);
	if (espFsData==MAP_FAILED) {
		perror("mmap");
		exit(1);
	}

	ir=espFsInit(espFsData);
	if (ir != ESPFS_INIT_RESULT_OK) {
		printf("Couldn't init espfs filesystem (code %d)\n", ir);
		exit(1);
	}

	ef=espFsOpen(argv[2]);
	if (ef==NULL) {
		printf("Couldn't find %s in image.\n", argv[2]);
		exit(1);
	}

	out=open(argv[2], O_WRONLY|O_CREAT|O_TRUNC, 0644);
	if (out<=0) {
		perror(argv[2]);
		exit(1);
	}
	
	while ((len=espFsRead(ef, buff, 128))!=0) {
		write(out, buff, len);
	}
	espFsClose(ef);
	//munmap, close, ... I can't be bothered.
}
