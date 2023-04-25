/*
 * UniString
 *
 * Copyright 2022 José Antonio Jiménez Campos (@RavenSystem)
 *
 */

#include <string.h>

#ifndef ESP_PLATFORM

#include <esplibs/libmain.h>

#endif

#include "unistring.h"

unsigned char* uni_memdup(unsigned char* string, size_t size, unistring_t** unistrings) {
    unistring_t* unistring = *unistrings;
    
    while (unistring &&
           (size != unistring->size ||
           memcmp(string, unistring->string, size))) {
        unistring = unistring->next;
    }
    
    if (unistring == NULL) {
        unistring = malloc(sizeof(unistring_t));
        memset(unistring, 0, sizeof(unistring_t));
        
        unistring->size = size;
        unistring->string = malloc(size);
        memcpy(unistring->string, string, size);
        
        unistring->next = *unistrings;
        *unistrings = unistring;
    }
    
    return unistring->string;
}

char* uni_strdup(char* string, unistring_t** unistrings) {
    return (char*) uni_memdup((unsigned char*) string, strlen(string) + 1, unistrings);
}

void unistring_destroy(unistring_t* unistrings) {
    unistring_t* unistring = unistrings;
    
    while (unistring) {
        unistring_t* current_unistring = unistring;
        unistring = unistring->next;
        
        /*
        char* str = malloc(current_unistring->size + 1);
        snprintf(str, current_unistring->size, "%s", current_unistring->string);
        printf("UNIString (%i): \"%s\"\n", current_unistring->size, str);
        free(str);
        */
        
        free(current_unistring);
    }
}
