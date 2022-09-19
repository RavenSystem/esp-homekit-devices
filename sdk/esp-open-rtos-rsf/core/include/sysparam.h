/*
 * Part of esp-open-rtos
 * Copyright (C) 2016 Alex Stewart
 * BSD Licensed as described in the file LICENSE
 */

#ifndef _SYSPARAM_H_
#define _SYSPARAM_H_

#include <esp/types.h>

#ifndef DEFAULT_SYSPARAM_SECTORS
#define DEFAULT_SYSPARAM_SECTORS 4
#endif

/** @file sysparam.h
 *
 *  Read/write "system parameters" to persistent flash.
 *
 *  System parameters are stored as key/value pairs.  Keys are string values
 *  between 1 and 65535 characters long.  Values can be any data up to 65535
 *  bytes in length (but are most commonly also text strings). Up to 126 key/
 *  value pairs can be stored at a time.
 *
 *  Keys and values are stored in flash using a progressive list structure
 *  which allows space-efficient storage and minimizes flash erase cycles,
 *  improving write speed and increasing the lifespan of the flash memory.
 */

/** Status codes returned by all sysparam functions
 *
 *  Error codes (`SYSPARAM_ERR_*`) all have values less than zero, and can be
 *  returned by any function.  Values greater than zero are non-error status
 *  codes which may be returned by some functions to indicate various results.
 */
typedef enum {
    SYSPARAM_OK           = 0,  ///< Success
    SYSPARAM_NOTFOUND     = 1,  ///< Entry not found matching criteria
    SYSPARAM_PARSEFAILED  = 2,  ///< Unable to parse retrieved value
    SYSPARAM_ERR_NOINIT   = -1, ///< sysparam_init() must be called first
    SYSPARAM_ERR_BADVALUE = -2, ///< One or more arguments were invalid
    SYSPARAM_ERR_FULL     = -3, ///< No space left in sysparam area (or too many keys in use)
    SYSPARAM_ERR_IO       = -4, ///< I/O error reading/writing flash
    SYSPARAM_ERR_CORRUPT  = -5, ///< Sysparam region has bad/corrupted data
    SYSPARAM_ERR_NOMEM    = -6, ///< Unable to allocate memory
} sysparam_status_t;

/** Structure used by sysparam_iter_next() to keep track of its current state
 * and return its results.  This should be initialized by calling
 * sysparam_iter_start() and cleaned up afterward by calling
 * sysparam_iter_end().
 */
typedef struct {
    char *key;
    uint8_t *value;
    size_t key_len;
    size_t value_len;
    bool binary;
    size_t bufsize;
    struct sysparam_context *ctx;
} sysparam_iter_t;

/** Initialize sysparam and set up the current area of flash to use.
 *
 *  This must be called (and return successfully) before any other sysparam
 *  routines (except sysparam_create_area()) are called.
 *
 *  This should normally be taken care of automatically on boot by the OS
 *  startup routines.  It may be necessary to call it specially, however, if
 *  the normal initialization failed, or after calling sysparam_create_area()
 *  to reformat the current area.
 *
 *  This routine will start at `base_addr` and scan all sectors up to
 *  `top_addr` looking for a valid sysparam area.  If `top_addr` is zero (or
 *  equal to `base_addr`, then only the sector at `base_addr` will be checked.
 *
 *  @param[in] base_addr  The flash address to start looking for the start of
 *                        the (already present) sysparam area
 *  @param[in] top_addr   The flash address to stop looking for the sysparam
 *                        area
 *
 *  @retval ::SYSPARAM_OK           Initialization successful.
 *  @retval ::SYSPARAM_NOTFOUND     The specified address does not appear to
 *                                  contain a sysparam area.  It may be
 *                                  necessary to call sysparam_create_area() to
 *                                  create one first.
 *  @retval ::SYSPARAM_ERR_CORRUPT  Sysparam region has bad/corrupted data
 *  @retval ::SYSPARAM_ERR_IO       I/O error reading/writing flash
 */
sysparam_status_t sysparam_init(uint32_t base_addr, uint32_t top_addr);

/** Create a new sysparam area in flash at the specified address.
 *
 *  By default, this routine will scan the specified area to make sure it
 *  appears to be empty (i.e. all 0xFF bytes) before setting it up as a new
 *  sysparam area.  If there appears to be other data already present, it will
 *  not overwrite it.  Setting `force` to `true` will cause it to clobber any
 *  existing data instead.
 *
 *  @param[in] base_addr   The flash address at which it should start
 *                         (must be a multiple of the sector size)
 *  @param[in] num_sectors The number of flash sectors to use for the sysparam
 *                         area.  This should be an even number >= 2.  Note
 *                         that the actual amount of useable parameter space
 *                         will be roughly half this amount.
 *  @param[in] force       Proceed even if the space does not appear to be empty
 *
 *  @retval ::SYSPARAM_OK           Area (re)created successfully.
 *  @retval ::SYSPARAM_NOTFOUND     `force` was not specified, and the area at
 *                                  `base_addr` appears to have other data.  No
 *                                  action taken.
 *  @retval ::SYSPARAM_ERR_BADVALUE The `num_sectors` value was not even (or
 *                                  was zero)
 *  @retval ::SYSPARAM_ERR_IO       I/O error reading/writing flash
 *
 *  Note: This routine can create a sysparam area in another location than the
 *  one currently being used, but does not change which area is currently used
 *  (you will need to call sysparam_init() again if you want to do that).  If
 *  you reformat the area currently being used, you will also need to call
 *  sysparam_init() again afterward before you will be able to continue using
 *  it.
 */
sysparam_status_t sysparam_create_area(uint32_t base_addr, uint16_t num_sectors, bool force);

/** Get the start address and size of the currently active sysparam area
 *
 *  Fills in `base_addr` and `num_sectors` with the location and size of the
 *  currently active sysparam area.  The returned values correspond to the
 *  arguments passed to the sysparam_create_area() call when the area was
 *  originally created.
 *
 *  @param[out] base_addr   The flash address at which the sysparam area starts
 *  @param[out] num_sectors The number of flash sectors used by the sysparam
 *                          area
 *
 *  @retval ::SYSPARAM_OK           Completed successfully
 *  @retval ::SYSPARAM_ERR_NOINIT   No current sysparam area is active
 */
sysparam_status_t sysparam_get_info(uint32_t *base_addr, uint32_t *num_sectors);

/** Compact the sysparam area.
 *
 *  This also flattens the log.
 *
 *  @retval ::SYSPARAM_OK           Completed successfully
 *  @retval ::SYSPARAM_ERR_NOINIT   No current sysparam area is active
 *  @retval ::SYSPARAM_ERR_CORRUPT  Sysparam region has bad/corrupted data
 *  @retval ::SYSPARAM_ERR_IO       I/O error reading/writing flash
 */
sysparam_status_t sysparam_compact();

/** Get the value associated with a key
 *
 *  This is the core "get value" function.  It will retrieve the value for the
 *  specified key in a freshly malloc()'d buffer and return it.  Raw values can
 *  contain any data (including zero bytes), so the `actual_length` parameter
 *  should be used to determine the length of the data in the buffer.
 *
 *  It is up to the caller to free() the returned buffer when done using it.
 *
 *  Note: If the status result is anything other than ::SYSPARAM_OK, the value
 *  in `destptr` is not changed.  This means it is possible to set a default
 *  value before calling this function which will be left as-is if a sysparam
 *  value could not be successfully read.
 *
 *  @param[in]  key            Key name (zero-terminated string)
 *  @param[out] destptr        Pointer to a location to hold the address of the
 *                             returned data buffer
 *  @param[out] actual_length  Pointer to a location to hold the length of the
 *                             returned data buffer (may be NULL)
 *  @param[out] is_binary      Pointer to a bool to hold whether the returned
 *                             value is "binary" or not (may be NULL)
 *
 *  @retval ::SYSPARAM_OK           Value successfully retrieved.
 *  @retval ::SYSPARAM_NOTFOUND     Key/value not found.  No buffer returned.
 *  @retval ::SYSPARAM_ERR_NOINIT   sysparam_init() must be called first
 *  @retval ::SYSPARAM_ERR_NOMEM    Unable to allocate memory
 *  @retval ::SYSPARAM_ERR_CORRUPT  Sysparam region has bad/corrupted data
 *  @retval ::SYSPARAM_ERR_IO       I/O error reading/writing flash
 */
sysparam_status_t sysparam_get_data(const char *key, uint8_t **destptr, size_t *actual_length, bool *is_binary);

/** Get the value associated with a key (static value buffer)
 *
 *  This performs the same function as sysparam_get_data() but without
 *  allocating memory for the result value.  It can thus be used before the heap
 *  has been configured or in other cases where using the heap would be a
 *  problem (i.e. in an OOM handler, etc).  It requires that the caller pass in
 *  a suitably sized buffer for the value to be read (if the supplied buffer is
 *  not large enough, the returned value will be truncated and the full required
 *  length will be returned in `actual_length`).
 *
 *  @param[in]  key            Key name (zero-terminated string)
 *  @param[in]  dest           Pointer to a buffer to hold the returned value.
 *  @param[in]  dest_size      Length of the supplied buffer in bytes.
 *  @param[out] actual_length  Pointer to a location to hold the actual length
 *                             of the data which was associated with the key
 *                             (may be NULL).
 *  @param[out] is_binary      Pointer to a bool to hold whether the returned
 *                             value is "binary" or not (may be NULL)
 *
 *  @retval ::SYSPARAM_OK           Value successfully retrieved
 *  @retval ::SYSPARAM_NOTFOUND     Key/value not found
 *  @retval ::SYSPARAM_ERR_NOINIT   sysparam_init() must be called first
 *  @retval ::SYSPARAM_ERR_NOMEM    The supplied buffer is too small
 *  @retval ::SYSPARAM_ERR_CORRUPT  Sysparam region has bad/corrupted data
 *  @retval ::SYSPARAM_ERR_IO       I/O error reading/writing flash
 */
sysparam_status_t sysparam_get_data_static(const char *key, uint8_t *dest, size_t dest_size, size_t *actual_length, bool *is_binary);

/** Get the string value associated with a key
 *
 *  This routine can be used if you know that the value in a key will (or at
 *  least should) be a string.  It will return a zero-terminated char buffer
 *  containing the value retrieved.
 *
 *  It is up to the caller to free() the returned buffer when done using it.
 *
 *  Note: If the status result is anything other than ::SYSPARAM_OK, the value
 *  in `destptr` is not changed.  This means it is possible to set a default
 *  value before calling this function which will be left as-is if a sysparam
 *  value could not be successfully read.
 *
 *  @param[in]  key      Key name (zero-terminated string)
 *  @param[out] destptr  Pointer to a location to hold the address of the
 *                       returned data buffer
 *
 *  @retval ::SYSPARAM_OK           Value successfully retrieved.
 *  @retval ::SYSPARAM_NOTFOUND     Key/value not found.
 *  @retval ::SYSPARAM_PARSEFAILED  The retrieved value was a binary value
 *  @retval ::SYSPARAM_ERR_NOINIT   sysparam_init() must be called first
 *  @retval ::SYSPARAM_ERR_NOMEM    Unable to allocate memory
 *  @retval ::SYSPARAM_ERR_CORRUPT  Sysparam region has bad/corrupted data
 *  @retval ::SYSPARAM_ERR_IO       I/O error reading/writing flash
 */
sysparam_status_t sysparam_get_string(const char *key, char **destptr);

/** Get the int32_t value associated with a key
 *
 *  This routine can be used if you know that the value in a key will (or at
 *  least should) be an int32_t value. This is done without allocating any
 *  memory.
 *
 *  Note: If the status result is anything other than ::SYSPARAM_OK, the value
 *  in `result` is not changed.  This means it is possible to set a default
 *  value before calling this function which will be left as-is if a sysparam
 *  value could not be successfully read.
 *
 *  @param[in]  key     Key name (zero-terminated string)
 *  @param[out] result  Pointer to a location to hold returned integer value
 *
 *  @retval ::SYSPARAM_OK           Value successfully retrieved.
 *  @retval ::SYSPARAM_NOTFOUND     Key/value not found.
 *  @retval ::SYSPARAM_PARSEFAILED  The retrieved value could not be parsed as
 *                                  an integer.
 *  @retval ::SYSPARAM_ERR_NOINIT   sysparam_init() must be called first
 *  @retval ::SYSPARAM_ERR_NOMEM    Unable to allocate memory
 *  @retval ::SYSPARAM_ERR_CORRUPT  Sysparam region has bad/corrupted data
 *  @retval ::SYSPARAM_ERR_IO       I/O error reading/writing flash
 */
sysparam_status_t sysparam_get_int32(const char *key, int32_t *result);

/** Get the int8_t value associated with a key
 *
 *  This routine can be used if you know that the value in a key will (or at
 *  least should) be a uint8_t binary value. This is done without allocating any
 *  memory.
 *
 *  Note: If the status result is anything other than ::SYSPARAM_OK, the value
 *  in `result` is not changed.  This means it is possible to set a default
 *  value before calling this function which will be left as-is if a sysparam
 *  value could not be successfully read.
 *
 *  @param[in]  key     Key name (zero-terminated string)
 *  @param[out] result  Pointer to a location to hold returned boolean value
 *
 *  @retval ::SYSPARAM_OK           Value successfully retrieved.
 *  @retval ::SYSPARAM_NOTFOUND     Key/value not found.
 *  @retval ::SYSPARAM_PARSEFAILED  The retrieved value could not be parsed as a
 *                                  boolean setting.
 *  @retval ::SYSPARAM_ERR_NOINIT   sysparam_init() must be called first
 *  @retval ::SYSPARAM_ERR_NOMEM    Unable to allocate memory
 *  @retval ::SYSPARAM_ERR_CORRUPT  Sysparam region has bad/corrupted data
 *  @retval ::SYSPARAM_ERR_IO       I/O error reading/writing flash
 */
sysparam_status_t sysparam_get_int8(const char *key, int8_t *result);

/** Get the boolean value associated with a key
 *
 *  This routine can be used if you know that the value in a key will (or at
 *  least should) be a boolean setting.  It will read the specified value as a
 *  text string and attempt to parse it as a boolean value.
 *
 *  It will recognize the following (case-insensitive) strings:
 *    * True: "yes", "y", "true", "t", "1"
 *    * False: "no", "n", "false", "f", "0"
 *
 *  Note: If the status result is anything other than ::SYSPARAM_OK, the value
 *  in `result` is not changed.  This means it is possible to set a default
 *  value before calling this function which will be left as-is if a sysparam
 *  value could not be successfully read.
 *
 *  @param[in]  key     Key name (zero-terminated string)
 *  @param[out] result  Pointer to a location to hold returned boolean value
 *
 *  @retval ::SYSPARAM_OK           Value successfully retrieved.
 *  @retval ::SYSPARAM_NOTFOUND     Key/value not found.
 *  @retval ::SYSPARAM_PARSEFAILED  The retrieved value could not be parsed as a
 *                                  boolean setting.
 *  @retval ::SYSPARAM_ERR_NOINIT   sysparam_init() must be called first
 *  @retval ::SYSPARAM_ERR_NOMEM    Unable to allocate memory
 *  @retval ::SYSPARAM_ERR_CORRUPT  Sysparam region has bad/corrupted data
 *  @retval ::SYSPARAM_ERR_IO       I/O error reading/writing flash
 */
sysparam_status_t sysparam_get_bool(const char *key, bool *result);

/** Set the value associated with a key
 *
 *  The supplied value can be any data, up to 255 bytes in length.  If `value`
 *  is NULL or `value_len` is 0, this is treated as a request to delete any
 *  current entry matching `key`. This is done without allocating any memory.
 *
 *  If `binary` is true, the data will be considered binary (unprintable) data,
 *  and this will be annotated in the saved entry.  This does not affect the
 *  saving or loading process in any way, but may be used by some applications
 *  to (for example) print binary data differently than text entries when
 *  printing parameter values.
 *
 *  @param[in] key        Key name (zero-terminated string)
 *  @param[in] value      Pointer to a buffer containing the value data
 *  @param[in] value_len  Length of the data in the buffer
 *  @param[in] binary     Whether the data should be considered "binary"
 *                        (unprintable) data
 *
 *  @retval ::SYSPARAM_OK           Value successfully set.
 *  @retval ::SYSPARAM_ERR_NOINIT   sysparam_init() must be called first
 *  @retval ::SYSPARAM_ERR_BADVALUE Either an empty key was provided or
 *                                  value_len is too large
 *  @retval ::SYSPARAM_ERR_FULL     No space left in sysparam area
 *                                  (or too many keys in use)
 *  @retval ::SYSPARAM_ERR_NOMEM    Unable to allocate memory
 *  @retval ::SYSPARAM_ERR_CORRUPT  Sysparam region has bad/corrupted data
 *  @retval ::SYSPARAM_ERR_IO       I/O error reading/writing flash
 */
sysparam_status_t sysparam_set_data(const char *key, const uint8_t *value, size_t value_len, bool binary);

/** Set a key's value from a string
 *
 *  Performs the same function as sysparam_set_data(), but accepts a
 *  zero-terminated string value instead.
 *
 *  @param[in] key        Key name (zero-terminated string)
 *  @param[in] value      Value to set (zero-terminated string)
 *
 *  @retval ::SYSPARAM_OK           Value successfully set.
 *  @retval ::SYSPARAM_ERR_BADVALUE Either an empty key was provided or the
 *                                  length of `value` is too large
 *  @retval ::SYSPARAM_ERR_FULL     No space left in sysparam area
 *                                  (or too many keys in use)
 *  @retval ::SYSPARAM_ERR_NOMEM    Unable to allocate memory
 *  @retval ::SYSPARAM_ERR_CORRUPT  Sysparam region has bad/corrupted data
 *  @retval ::SYSPARAM_ERR_IO       I/O error reading/writing flash
 */
sysparam_status_t sysparam_set_string(const char *key, const char *value);

/** Set a key's value as a number
 *
 *  Write an int32_t binary value to the specified key. This does the inverse of
 *  the sysparam_get_int32() function. This is done without allocating any
 *  memory.
 *
 *  @param[in] key        Key name (zero-terminated string)
 *  @param[in] value      Value to set
 *
 *  @retval ::SYSPARAM_OK           Value successfully set.
 *  @retval ::SYSPARAM_ERR_BADVALUE An empty key was provided.
 *  @retval ::SYSPARAM_ERR_FULL     No space left in sysparam area
 *                                  (or too many keys in use)
 *  @retval ::SYSPARAM_ERR_NOMEM    Unable to allocate memory
 *  @retval ::SYSPARAM_ERR_CORRUPT  Sysparam region has bad/corrupted data
 *  @retval ::SYSPARAM_ERR_IO       I/O error reading/writing flash
 */
sysparam_status_t sysparam_set_int32(const char *key, int32_t value);

/** Set a key's value as a number
 *
 *  Write an int8_t binary value to the specified key. This does the inverse of
 *  the sysparam_get_int8() function. This is done without allocating any
 *  memory.
 *
 *  @param[in] key        Key name (zero-terminated string)
 *  @param[in] value      Value to set
 *
 *  @retval ::SYSPARAM_OK           Value successfully set.
 *  @retval ::SYSPARAM_ERR_BADVALUE An empty key was provided.
 *  @retval ::SYSPARAM_ERR_FULL     No space left in sysparam area
 *                                  (or too many keys in use)
 *  @retval ::SYSPARAM_ERR_NOMEM    Unable to allocate memory
 *  @retval ::SYSPARAM_ERR_CORRUPT  Sysparam region has bad/corrupted data
 *  @retval ::SYSPARAM_ERR_IO       I/O error reading/writing flash
 */
sysparam_status_t sysparam_set_int8(const char *key, int8_t value);

/** Set a key's value as a boolean (yes/no) string
 *
 *  Converts a bool value to a corresponding text string and writes it to the
 *  specified key.  This does the inverse of the sysparam_get_bool()
 *  function.
 *
 *  @param[in] key        Key name (zero-terminated string)
 *  @param[in] value      Value to set
 *
 *  @retval ::SYSPARAM_OK           Value successfully set.
 *  @retval ::SYSPARAM_ERR_BADVALUE An empty key was provided.
 *  @retval ::SYSPARAM_ERR_FULL     No space left in sysparam area
 *                                  (or too many keys in use)
 *  @retval ::SYSPARAM_ERR_NOMEM    Unable to allocate memory
 *  @retval ::SYSPARAM_ERR_CORRUPT  Sysparam region has bad/corrupted data
 *  @retval ::SYSPARAM_ERR_IO       I/O error reading/writing flash
 */
sysparam_status_t sysparam_set_bool(const char *key, bool value);

/** Begin iterating through all key/value pairs
 *
 *  This function initializes a sysparam_iter_t structure to prepare it for
 *  iterating through the list of key/value pairs using sysparam_iter_next().
 *  This does not fetch any items (the first successive call to
 *  sysparam_iter_next() will return the first key/value in the list).
 *
 *  NOTE: When done, you must call sysparam_iter_end() to free the resources
 *  associated with `iter`, or you will leak memory.
 *
 *  @param[in] iter  A pointer to a sysparam_iter_t structure to initialize
 *
 *  @retval ::SYSPARAM_OK           Initialization successful
 *  @retval ::SYSPARAM_ERR_NOMEM    Unable to allocate memory
 */
sysparam_status_t sysparam_iter_start(sysparam_iter_t *iter);

/** Fetch the next key/value pair
 *
 *  This will retrieve the next key and value from the sysparam area, placing
 *  them in `iter->key`, and `iter->value` (and updating `iter->key_len` and
 *  `iter->value_len`).
 *
 *  NOTE: `iter->key` and `iter->value` are static buffers local to the `iter`
 *  structure, and will be overwritten with the next call to
 *  sysparam_iter_next() using the same `iter`.  They should *not* be free()d
 *  after use.
 *
 *  @param[in] iter  The iterator structure to update
 *
 *  @retval ::SYSPARAM_OK           Next key/value retrieved
 *  @retval ::SYSPARAM_ERR_NOMEM    Unable to allocate memory
 *  @retval ::SYSPARAM_ERR_CORRUPT  Sysparam region has bad/corrupted data
 *  @retval ::SYSPARAM_ERR_IO       I/O error reading/writing flash
 */
sysparam_status_t sysparam_iter_next(sysparam_iter_t *iter);

/** Finish iterating through keys/values
 *
 *  Cleans up and releases resources allocated by sysparam_iter_start() /
 *  sysparam_iter_next().
 */
void sysparam_iter_end(sysparam_iter_t *iter);

#endif /* _SYSPARAM_H_ */
