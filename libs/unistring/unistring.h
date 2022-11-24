/*
* UniString
*
* Copyright 2022 José Antonio Jiménez Campos (@RavenSystem)
*
*/

#ifndef __UNISTRING_H__
#define __UNISTRING_H__

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _unistring {
    size_t size;
    unsigned char* string;
    
    struct _unistring* next;
} unistring_t;

unsigned char* uni_memdup(unsigned char* string, size_t size, unistring_t** unistrings);
char* uni_strdup(char* string, unistring_t** unistrings);
void unistring_destroy(unistring_t* unistrings);

#ifdef __cplusplus
}
#endif

#endif  // __ADV_HLW_H__
