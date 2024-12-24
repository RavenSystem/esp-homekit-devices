#pragma once

#include <stdbool.h>

#define JSON_STATE_START            (1)
#define JSON_STATE_END              (2)
#define JSON_STATE_OBJECT           (3)
#define JSON_STATE_OBJECT_KEY       (4)
#define JSON_STATE_OBJECT_VALUE     (5)
#define JSON_STATE_ARRAY            (6)
#define JSON_STATE_ARRAY_ITEM       (7)
#define JSON_STATE_ERROR            (8)

#define JSON_MAX_DEPTH              (30)

#define HOMEKIT_JSON_BUFFER_SIZE    (1024)


typedef int (*json_flush_callback)(uint8_t *buffer, size_t size, void *context);
typedef struct _json_stream {
    uint8_t* buffer;
    size_t pos;

    uint8_t state: 7;
    bool error: 1;

    uint8_t nesting_idx;
    uint8_t nesting[JSON_MAX_DEPTH];

    json_flush_callback on_flush;
    void *context;
} json_stream;


void json_init(json_stream *json, void *context);
//json_stream *json_new(size_t buffer_size, uint8_t* buffer_data, json_flush_callback on_flush, void *context);
void json_buffer_free(json_stream *json);

void json_flush(json_stream *json);

void json_object_start(json_stream *json);
void json_object_end(json_stream *json);

void json_array_start(json_stream *json);
void json_array_end(json_stream *json);

void json_integer(json_stream *json, long int x);
void json_float(json_stream *json, float x);
void json_string(json_stream *json, const char *x);
void json_boolean(json_stream *json, bool x);
void json_null(json_stream *json);

