#include <stdio.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>

//Cygwin e.a. needs O_BINARY. Don't miscompile if it's not set.
#ifndef O_BINARY
#define O_BINARY 0
#endif

typedef struct __attribute__((packed)) {
	char magic[4];
	char tag[28];
	int32_t len1;
	int32_t len2;
} Header;


int openFile(char *file) {
	int r=open(file, O_RDONLY|O_BINARY);
	if (r<=0) {
		perror(file);
		exit(1);
	}
	return r;
}

int32_t intToEsp(int32_t v) {
	int32_t ret;
	char *p=(char*)&ret;
	*p++=(v>>0)&0xff;
	*p++=(v>>8)&0xff;
	*p++=(v>>16)&0xff;
	*p++=(v>>24)&0xff;
	return ret;
}

size_t fileLen(int f) {
	size_t r;
	r=lseek(f, 0, SEEK_END);
	lseek(f, 0, SEEK_SET);
	return r;
}


int main(int argc, char **argv) {
	int u1, u2;
	size_t l1, l2;
	int of;
	char *fc1, *fc2;
	Header hdr;

	memset(&hdr, 0, sizeof(hdr));
#if RBOOT_OTA==1
	if (argc!=4) {
		printf("Usage: %s user.bin tagname ota_firmware.bin\n", argv[0]);
		exit(1);
	}
	if (strlen(argv[2])>27) {
		printf("Error: Tag can't be longer than 27 characters.\n");
		exit(1);
	}
	strcpy(hdr.tag, argv[2]);
	of=open(argv[3], O_WRONLY|O_CREAT|O_TRUNC|O_BINARY, 0666);
#else
	if (argc!=5) {
		printf("Usage: %s user1.bin user2.bin tagname outfile.bin\n", argv[0]);
		exit(1);
	}
	if (strlen(argv[3])>27) {
		printf("Error: Tag can't be longer than 27 characters.\n");
		exit(1);
	}
	strcpy(hdr.tag, argv[3]);
	of=open(argv[4], O_WRONLY|O_CREAT|O_TRUNC|O_BINARY, 0666);
#endif
	memcpy(hdr.magic, "EHUG", 4);

	if (of<=0) {
		perror(argv[4]);
		exit(1);
	}

	u1=openFile(argv[1]);
	l1=fileLen(u1);
	hdr.len1=intToEsp(l1);
	fc1=malloc(l1);
	if (read(u1, fc1, l1)!=l1) {
		perror(argv[1]);
		exit(1);
	}
	close(u1);

#if RBOOT_OTA==0
	u2=openFile(argv[2]);
	l2=fileLen(u2);
	hdr.len2=intToEsp(l2);
	fc2=malloc(l2);
	if (read(u2, fc2, l2)!=l2) {
		perror(argv[2]);
		exit(1);
	}
	close(u2);
#endif

	write(of, &hdr, sizeof(hdr));
	write(of, fc1, l1);
#if RBOOT_OTA==0
	write(of, fc2, l2);
	printf("Header: %d bytes, user1: %d bytes, user2: %d bytes.\n", sizeof(hdr), (int)l1, (int)l2);
#else
	printf("Header: %d bytes, user: %d bytes.\n", sizeof(hdr), (int)l1);
#endif
	close(of);

	exit(0);
}

