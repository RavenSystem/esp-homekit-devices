#ifndef _DIRENT_H_
#define _DIRENT_H_
#ifdef __cplusplus
extern "C" {
#endif
#include <sys/cdefs.h>
#include <sys/dirent.h>

#if !defined(MAXNAMLEN) && __BSD_VISIBLE
#define MAXNAMLEN 1024
#endif

#ifdef __cplusplus
}
#endif
#endif /*_DIRENT_H_*/
