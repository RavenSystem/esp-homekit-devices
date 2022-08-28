/* base64.c : base-64 / MIME encode/decode */
/* PUBLIC DOMAIN - Jon Mayo - November 13, 2003 */

#include <ctype.h>
#include <esp8266.h>
#include <libesphttpd/base64.h>

IROM static const int base64dec_tab[256] ={
	255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
	255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
	255,255,255,255,255,255,255,255,255,255,255, 62,255,255,255, 63,
	 52, 53, 54, 55, 56, 57, 58, 59, 60, 61,255,255,255,  0,255,255,
	255,  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14,
	 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25,255,255,255,255,255,
	255, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40,
	 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51,255,255,255,255,255,
	255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
	255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
	255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
	255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
	255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
	255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
	255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
	255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
};

#if 0
static int base64decode(const char in[4], char out[3]) {
	uint8_t v[4];

	v[0]=base64dec_tab[(unsigned)in[0]];
	v[1]=base64dec_tab[(unsigned)in[1]];
	v[2]=base64dec_tab[(unsigned)in[2]];
	v[3]=base64dec_tab[(unsigned)in[3]];

	out[0]=(v[0]<<2)|(v[1]>>4); 
	out[1]=(v[1]<<4)|(v[2]>>2); 
	out[2]=(v[2]<<6)|(v[3]); 
	return (v[0]|v[1]|v[2]|v[3])!=255 ? in[3]=='=' ? in[2]=='=' ? 1 : 2 : 3 : 0;
}
#endif

/* decode a base64 string in one shot */
int  __attribute__((weak)) base64_decode(size_t in_len, const char *in, size_t out_len, unsigned char *out) {
	unsigned int ii, io;
	uint32_t v;
	unsigned int rem;

	for(io=0,ii=0,v=0,rem=0;ii<in_len;ii++) {
		unsigned char ch;
		if(isspace((int)in[ii])) continue;
		if(in[ii]=='=') break; /* stop at = */
		ch=base64dec_tab[(unsigned int)in[ii]];
		if(ch==255) break; /* stop at a parse error */
		v=(v<<6)|ch;
		rem+=6;
		if(rem>=8) {
			rem-=8;
			if(io>=out_len) return -1; /* truncation is failure */
			out[io++]=(v>>rem)&255;
		}
	}
	if(rem>=8) {
		rem-=8;
		if(io>=out_len) return -1; /* truncation is failure */
		out[io++]=(v>>rem)&255;
	}
	return io;
}

static const uint8_t base64enc_tab[64]= "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

#if 0
void base64encode(const unsigned char in[3], unsigned char out[4], int count) {
	out[0]=base64enc_tab[(in[0]>>2)];
	out[1]=base64enc_tab[((in[0]&3)<<4)|(in[1]>>4)];
	out[2]=count<2 ? '=' : base64enc_tab[((in[1]&15)<<2)|(in[2]>>6)];
	out[3]=count<3 ? '=' : base64enc_tab[(in[2]&63)];
}
#endif

int __attribute__((weak)) base64_encode(size_t in_len, const unsigned char *in, size_t out_len, char *out) {
	unsigned ii, io;
	uint32_t v;
	unsigned rem;

	for(io=0,ii=0,v=0,rem=0;ii<in_len;ii++) {
		unsigned char ch;
		ch=in[ii];
		v=(v<<8)|ch;
		rem+=8;
		while(rem>=6) {
			rem-=6;
			if(io>=out_len) return -1; /* truncation is failure */
			out[io++]=base64enc_tab[(v>>rem)&63];
		}
	}
	if(rem) {
		v<<=(6-rem);
		if(io>=out_len) return -1; /* truncation is failure */
		out[io++]=base64enc_tab[v&63];
	}
	while(io&3) {
		if(io>=out_len) return -1; /* truncation is failure */
		out[io++]='=';
	}
	if(io>=out_len) return -1; /* no room for null terminator */
	out[io]=0;
	return io;
}

