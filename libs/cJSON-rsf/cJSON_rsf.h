/*
 * cJSON RavenSystem Fork
 *
 * Copyright 2023 José Antonio Jiménez Campos (@RavenSystem)
 *
 */

/*
  Copyright (c) 2009-2017 Dave Gamble and cJSON_rsf contributors

  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files (the "Software"), to deal
  in the Software without restriction, including without limitation the rights
  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
  copies of the Software, and to permit persons to whom the Software is
  furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in
  all copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
  THE SOFTWARE.
*/

#ifndef cJSON_rsf__h
#define cJSON_rsf__h

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdint.h>
#include <stdbool.h>

/* cJSON_rsf Types: */
#define cJSON_rsf_Invalid (0)
#define cJSON_rsf_False  (1 << 0)
#define cJSON_rsf_True   (1 << 1)
#define cJSON_rsf_NULL   (1 << 2)
#define cJSON_rsf_Number (1 << 3)
#define cJSON_rsf_String (1 << 4)
#define cJSON_rsf_Array  (1 << 5)
#define cJSON_rsf_Object (1 << 6)
#define cJSON_rsf_Raw    (1 << 7) /* raw json */

#define cJSON_rsf_IsReference 256
#define cJSON_rsf_StringIsConst 512

/* The cJSON_rsf structure: */
typedef struct _cJSON_rsf {
    /* The type of the item, as above. */
    int32_t type;
    
    /* The item's name string, if this item is the child of, or is in the list of subitems of an object. */
    char* string;
    
    union {
        /* The item's string, if type==cJSON_rsf_String  or type == cJSON_rsf_Raw */
        char* valuestring;
        /* The item's number, if type==cJSON_rsf_Number */
        float valuefloat;
    };
    
    /* next/prev allow you to walk array/object chains. Alternatively, use GetArraySize/GetArrayItem/GetObjectItem */
    struct _cJSON_rsf *next;
    struct _cJSON_rsf *prev;
    
    /* An array or object item will have a child pointer pointing to a chain of the items in the array/object. */
    struct _cJSON_rsf *child;
} cJSON_rsf;

/* Limits how deeply nested arrays/objects can be before cJSON_rsf rejects to parse them.
 * This is to prevent stack overflows. */
#ifndef CJSON_NESTING_LIMIT
#define CJSON_NESTING_LIMIT 1000
#endif

/* Supply a block of JSON, and this returns a cJSON_rsf object you can interrogate. */
cJSON_rsf* cJSON_rsf_Parse(const char *value);
/* ParseWithOpts allows you to require (and check) that the JSON is null terminated, and to retrieve the pointer to the final byte parsed. */
cJSON_rsf* cJSON_rsf_ParseWithOpts(const char *value, bool require_null_terminated);

/* Render a cJSON_rsf entity to text for transfer/storage. */
char* cJSON_rsf_Print(const cJSON_rsf *item);
/* Render a cJSON_rsf entity to text for transfer/storage without any formatting. */
char* cJSON_rsf_PrintUnformatted(const cJSON_rsf *item);
/* Render a cJSON_rsf entity to text using a buffered strategy. prebuffer is a guess at the final size. guessing well reduces reallocation. fmt=0 gives unformatted, =1 gives formatted */
char* cJSON_rsf_PrintBuffered(const cJSON_rsf *item, int prebuffer, bool fmt);
/* Render a cJSON_rsf entity to text using a buffer already allocated in memory with given length. Returns 1 on success and 0 on failure. */
/* NOTE: cJSON_rsf is not always 100% accurate in estimating how much memory it will use, so to be safe allocate 5 bytes more than you actually need */
bool cJSON_rsf_PrintPreallocated(cJSON_rsf *item, char *buffer, const int length, const bool format);
/* Delete a cJSON_rsf entity and all subentities. */
void cJSON_rsf_Delete(cJSON_rsf *c);

/* Returns the number of items in an array (or object). */
int cJSON_rsf_GetArraySize(const cJSON_rsf *array);
/* Retrieve item number "index" from array "array". Returns NULL if unsuccessful. */
cJSON_rsf* cJSON_rsf_GetArrayItem(const cJSON_rsf *array, int index);
/* Get item "string" from object. Case insensitive. */
cJSON_rsf* cJSON_rsf_GetObjectItem(const cJSON_rsf * const object, const char * const string);
cJSON_rsf* cJSON_rsf_GetObjectItemCaseSensitive(const cJSON_rsf * const object, const char * const string);
bool cJSON_rsf_HasObjectItem(const cJSON_rsf *object, const char *string);

/* Check if the item is a string and return its valuestring */
char* cJSON_rsf_GetStringValue(cJSON_rsf *item);

/* These functions check the type of an item */
bool cJSON_rsf_IsInvalid(const cJSON_rsf * const item);
bool cJSON_rsf_IsFalse(const cJSON_rsf * const item);
bool cJSON_rsf_IsTrue(const cJSON_rsf * const item);
bool cJSON_rsf_IsBool(const cJSON_rsf * const item);
bool cJSON_rsf_IsNull(const cJSON_rsf * const item);
bool cJSON_rsf_IsNumber(const cJSON_rsf * const item);
bool cJSON_rsf_IsString(const cJSON_rsf * const item);
bool cJSON_rsf_IsArray(const cJSON_rsf * const item);
bool cJSON_rsf_IsObject(const cJSON_rsf * const item);
bool cJSON_rsf_IsRaw(const cJSON_rsf * const item);

/* These calls create a cJSON_rsf item of the appropriate type. */
cJSON_rsf* cJSON_rsf_CreateNull(void);
cJSON_rsf* cJSON_rsf_CreateTrue(void);
cJSON_rsf* cJSON_rsf_CreateFalse(void);
cJSON_rsf* cJSON_rsf_CreateBool(bool boolean);
cJSON_rsf* cJSON_rsf_CreateNumber(float num);
cJSON_rsf* cJSON_rsf_CreateString(const char *string);
/* raw json */
cJSON_rsf* cJSON_rsf_CreateRaw(const char *raw);
cJSON_rsf* cJSON_rsf_CreateArray(void);
cJSON_rsf* cJSON_rsf_CreateObject(void);

/* Create a string where valuestring references a string so
 * it will not be freed by cJSON_rsf_Delete */
cJSON_rsf* cJSON_rsf_CreateStringReference(const char *string);
/* Create an object/arrray that only references it's elements so
 * they will not be freed by cJSON_rsf_Delete */
cJSON_rsf* cJSON_rsf_CreateObjectReference(const cJSON_rsf *child);
cJSON_rsf* cJSON_rsf_CreateArrayReference(const cJSON_rsf *child);

/* These utilities create an Array of count items. */
cJSON_rsf* cJSON_rsf_CreateIntArray(const int *numbers, int count);
cJSON_rsf* cJSON_rsf_CreateFloatArray(const float *numbers, int count);
cJSON_rsf* cJSON_rsf_CreateDoubleArray(const double *numbers, int count);
cJSON_rsf* cJSON_rsf_CreateStringArray(const char **strings, int count);

/* Append item to the specified array/object. */
void cJSON_rsf_AddItemToArray(cJSON_rsf *array, cJSON_rsf *item);
void cJSON_rsf_AddItemToObject(cJSON_rsf *object, const char *string, cJSON_rsf *item);
/* Use this when string is definitely const (i.e. a literal, or as good as), and will definitely survive the cJSON_rsf object.
 * WARNING: When this function was used, make sure to always check that (item->type & cJSON_rsf_StringIsConst) is zero before
 * writing to `item->string` */
void cJSON_rsf_AddItemToObjectCS(cJSON_rsf *object, const char *string, cJSON_rsf *item);
/* Append reference to item to the specified array/object. Use this when you want to add an existing cJSON_rsf to a new cJSON_rsf, but don't want to corrupt your existing cJSON_rsf. */
void cJSON_rsf_AddItemReferenceToArray(cJSON_rsf *array, cJSON_rsf *item);
void cJSON_rsf_AddItemReferenceToObject(cJSON_rsf *object, const char *string, cJSON_rsf *item);

/* Remove/Detatch items from Arrays/Objects. */
cJSON_rsf* cJSON_rsf_DetachItemViaPointer(cJSON_rsf *parent, cJSON_rsf * const item);
cJSON_rsf* cJSON_rsf_DetachItemFromArray(cJSON_rsf *array, int which);
void cJSON_rsf_DeleteItemFromArray(cJSON_rsf *array, int which);
cJSON_rsf* cJSON_rsf_DetachItemFromObject(cJSON_rsf *object, const char *string);
cJSON_rsf* cJSON_rsf_DetachItemFromObjectCaseSensitive(cJSON_rsf *object, const char *string);
void cJSON_rsf_DeleteItemFromObject(cJSON_rsf *object, const char *string);
void cJSON_rsf_DeleteItemFromObjectCaseSensitive(cJSON_rsf *object, const char *string);

/* Update array items. */
void cJSON_rsf_InsertItemInArray(cJSON_rsf *array, int which, cJSON_rsf *newitem); /* Shifts pre-existing items to the right. */
bool cJSON_rsf_ReplaceItemViaPointer(cJSON_rsf * const parent, cJSON_rsf * const item, cJSON_rsf * replacement);
void cJSON_rsf_ReplaceItemInArray(cJSON_rsf *array, int which, cJSON_rsf *newitem);
void cJSON_rsf_ReplaceItemInObject(cJSON_rsf *object,const char *string,cJSON_rsf *newitem);
void cJSON_rsf_ReplaceItemInObjectCaseSensitive(cJSON_rsf *object,const char *string,cJSON_rsf *newitem);

/* Duplicate a cJSON_rsf item */
cJSON_rsf* cJSON_rsf_Duplicate(const cJSON_rsf *item, bool recurse);
/* Duplicate will create a new, identical cJSON_rsf item to the one you pass, in new memory that will
need to be released. With recurse!=0, it will duplicate any children connected to the item.
The item->next and ->prev pointers are always zero on return from Duplicate. */
/* Recursively compare two cJSON_rsf items for equality. If either a or b is NULL or invalid, they will be considered unequal.
 * case_sensitive determines if object keys are treated case sensitive (1) or case insensitive (0) */
bool cJSON_rsf_Compare(const cJSON_rsf * const a, const cJSON_rsf * const b, const bool case_sensitive);


void cJSON_rsf_Minify(char *json);

/* Helper functions for creating and adding items to an object at the same time.
 * They return the added item or NULL on failure. */
cJSON_rsf* cJSON_rsf_AddNullToObject(cJSON_rsf * const object, const char * const name);
cJSON_rsf* cJSON_rsf_AddTrueToObject(cJSON_rsf * const object, const char * const name);
cJSON_rsf* cJSON_rsf_AddFalseToObject(cJSON_rsf * const object, const char * const name);
cJSON_rsf* cJSON_rsf_AddBoolToObject(cJSON_rsf * const object, const char * const name, const bool boolean);
cJSON_rsf* cJSON_rsf_AddNumberToObject(cJSON_rsf * const object, const char * const name, const float number);
cJSON_rsf* cJSON_rsf_AddStringToObject(cJSON_rsf * const object, const char * const name, const char * const string);
cJSON_rsf* cJSON_rsf_AddRawToObject(cJSON_rsf * const object, const char * const name, const char * const raw);
cJSON_rsf* cJSON_rsf_AddObjectToObject(cJSON_rsf * const object, const char * const name);
cJSON_rsf* cJSON_rsf_AddArrayToObject(cJSON_rsf * const object, const char * const name);

/* When assigning an integer value, it needs to be propagated to valuefloat. */
#define cJSON_rsf_SetIntValue(object, number) ((object) ? (object)->valuefloat = (number) : (number))
/* helper for the cJSON_rsf_SetNumberValue macro */
float cJSON_rsf_SetNumberHelper(cJSON_rsf *object, float number);
#define cJSON_rsf_SetNumberValue(object, number) ((object != NULL) ? cJSON_rsf_SetNumberHelper(object, (float) number) : (number))

/* Macro for iterating over an array or object */
#define cJSON_rsf_ArrayForEach(element, array) for(element = (array != NULL) ? (array)->child : NULL; element != NULL; element = element->next)

/* malloc/free objects using the malloc/free functions that have been set with cJSON_rsf_InitHooks */
void* cJSON_rsf_malloc(size_t size);
void cJSON_rsf_free(void *object);

#ifdef __cplusplus
}
#endif

#endif
