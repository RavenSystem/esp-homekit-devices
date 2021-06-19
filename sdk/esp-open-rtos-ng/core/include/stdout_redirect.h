/*
 * Part of esp-open-rtos
 * Copyright (C) 2016 Oto Petrik <oto.petrik@gmail.com>
 * BSD Licensed as described in the file LICENSE
 */

#ifndef _STDOUT_REDIRECT_H_
#define _STDOUT_REDIRECT_H_

#include <sys/reent.h>

#ifdef __cplusplus
extern "C"
{
#endif

typedef ssize_t _WriteFunction(struct _reent *r, int fd, const void *ptr, size_t len);

/** Set implementation of write syscall for stdout.
 *
 *  Use this function to redirect stdout for further processing (save to file, send over network).
 *  It may be good idea to save result of `get_write_stdout()` beforehand and call
 *  it at the end of the supplied function.
 *
 *  NOTE: use NULL to reset to default implementation.
 *
 *  @param[in] f  New code to handle stdout output
 *
 */
void set_write_stdout(_WriteFunction *f);

/** Get current implementation of write syscall for stdout.
 *
 *  Save returned value before calling `set_write_stdout`, it allows for chaining
 *  multiple independent handlers.
 *
 *  @returns current stdout handler
 */
_WriteFunction *get_write_stdout();

#ifdef __cplusplus
}
#endif

#endif /* _STDOUT_REDIRECT_H_ */
