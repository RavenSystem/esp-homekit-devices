#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include "json.h"
#include "debug.h"

#define MAX(a, b)           (((a) > (b)) ? (a) : (b))

#define DEBUG_STATE(json) \
    DEBUG("State = %d, last JSON output: %s", \
          json->state, json->buffer + MAX(0, (long int)json->pos - 20));

#define JSON_NESTING_OBJECT (0)
#define JSON_NESTING_ARRAY  (1)


void json_init(json_stream *json, void *context) {
    json->pos = 0;
    json->state = JSON_STATE_START;
    json->error = false;
    json->nesting_idx = 0;
    json->context = context;
}

json_stream *json_new(size_t buffer_size, uint8_t* buffer_data, json_flush_callback on_flush, void *context) {
    json_stream* json = malloc(sizeof(json_stream));
    if (json) {
        json->size = buffer_size;
        if (buffer_data) {
            json->buffer = buffer_data;
        } else {
            json->buffer = malloc(json->size);
        }
        json->on_flush = on_flush;
        json_init(json, context);
        if (!json->buffer) {
            free(json);
            json = NULL;
        }
    }
    
    return json;
}

void json_buffer_free(json_stream *json) {
    free(json->buffer);
    free(json);
}

void json_flush(json_stream *json) {
    if (!json->pos)
        return;

    if (json->on_flush) {
        if (json->on_flush(json->buffer, json->pos, json->context) < 0) {
            json->state = JSON_STATE_ERROR;
            json->error = true;
        }
    }
    
    json->pos = 0;
}

void json_write(json_stream *json, const char *format, ...) {
    va_list arg_ptr;

    va_start(arg_ptr, format);
    int len = vsnprintf((char *)json->buffer + json->pos, json->size - json->pos, format, arg_ptr);
    va_end(arg_ptr);

    if (len + json->pos > json->size - 1) {
        json_flush(json);
        
        if (json->error) {
            return;
        }

        va_start(arg_ptr, format);
        int len = vsnprintf((char *)json->buffer + json->pos, json->size - json->pos, format, arg_ptr);
        va_end(arg_ptr);
        
        if (len > json->size - 1) {
            ERROR("Write value too large %i/%i", len, json->size);
            DEBUG("Format = %s", format);
            DEBUG("Data = %s", (char *)json->buffer);
        } else {
            json->pos += len;
        }
    } else {
        json->pos += len;
    }
}

void json_object_start(json_stream *json) {
    if (json->error)
        json->state = JSON_STATE_ERROR;
    if (json->state == JSON_STATE_ERROR)
        return;

    switch (json->state) {
        case JSON_STATE_ARRAY_ITEM:
            json_write(json, ",");
        case JSON_STATE_START:
        case JSON_STATE_OBJECT_KEY:
        case JSON_STATE_ARRAY:
            json_write(json, "{");

            json->state = JSON_STATE_OBJECT;
            json->nesting[json->nesting_idx++] = JSON_NESTING_OBJECT;
            break;
        default:
            ERROR("Unexpected object start");
            DEBUG_STATE(json);
            json->state = JSON_STATE_ERROR;
    }
}

void json_object_end(json_stream *json) {
    if (json->error)
        json->state = JSON_STATE_ERROR;
    if (json->state == JSON_STATE_ERROR)
        return;

    switch (json->state) {
        case JSON_STATE_OBJECT:
        case JSON_STATE_OBJECT_VALUE:
            json_write(json, "}");

            json->nesting_idx--;
            if (!json->nesting_idx) {
                json->state = JSON_STATE_END;
            } else {
                switch (json->nesting[json->nesting_idx-1]) {
                    case JSON_NESTING_OBJECT:
                        json->state = JSON_STATE_OBJECT_VALUE;
                        break;
                    case JSON_NESTING_ARRAY:
                        json->state = JSON_STATE_ARRAY_ITEM;
                        break;
                }
            }
            break;
        default:
            ERROR("Unexpected object end");
            DEBUG_STATE(json);
            json->state = JSON_STATE_ERROR;
    }
}

void json_array_start(json_stream *json) {
    if (json->error)
        json->state = JSON_STATE_ERROR;
    if (json->state == JSON_STATE_ERROR)
        return;

    switch (json->state) {
        case JSON_STATE_ARRAY_ITEM:
            json_write(json, ",");
        case JSON_STATE_START:
        case JSON_STATE_OBJECT_KEY:
        case JSON_STATE_ARRAY:
            json_write(json, "[");

            json->state = JSON_STATE_ARRAY;
            json->nesting[json->nesting_idx++] = JSON_NESTING_ARRAY;
            break;
        default:
            ERROR("Unexpected array start");
            DEBUG_STATE(json);
            json->state = JSON_STATE_ERROR;
    }
}

void json_array_end(json_stream *json) {
    if (json->error)
        json->state = JSON_STATE_ERROR;
    if (json->state == JSON_STATE_ERROR)
        return;

    switch (json->state) {
        case JSON_STATE_ARRAY:
        case JSON_STATE_ARRAY_ITEM:
            json_write(json, "]");

            json->nesting_idx--;
            if (!json->nesting_idx) {
                json->state = JSON_STATE_END;
            } else {
                switch (json->nesting[json->nesting_idx-1]) {
                    case JSON_NESTING_OBJECT:
                        json->state = JSON_STATE_OBJECT_VALUE;
                        break;
                    case JSON_NESTING_ARRAY:
                        json->state = JSON_STATE_ARRAY_ITEM;
                        break;
                }
            }
            break;
        default:
            ERROR("Unexpected array end");
            DEBUG_STATE(json);
            json->state = JSON_STATE_ERROR;
    }
}

void json_integer(json_stream *json, long long x) {
    if (json->error)
        json->state = JSON_STATE_ERROR;
    if (json->state == JSON_STATE_ERROR)
        return;

    void _do_write() {
        json_write(json, "%ld", x);
    }

    switch (json->state) {
        case JSON_STATE_START:
            _do_write();
            json->state = JSON_STATE_END;
            break;
        case JSON_STATE_ARRAY_ITEM:
            json_write(json, ",");
        case JSON_STATE_ARRAY:
            _do_write();
            json->state = JSON_STATE_ARRAY_ITEM;
            break;
        case JSON_STATE_OBJECT_KEY:
            _do_write();
            json->state = JSON_STATE_OBJECT_VALUE;
            break;
        default:
            ERROR("Unexpected integer");
            DEBUG_STATE(json);
            json->state = JSON_STATE_ERROR;
    }
}

void json_float(json_stream *json, float x) {
    if (json->error)
        json->state = JSON_STATE_ERROR;
    if (json->state == JSON_STATE_ERROR)
        return;

    void _do_write() {
        //json_write(json, "%1.15g", x);
        json_write(json, "%1.7g", x);
    }

    switch (json->state) {
        case JSON_STATE_START:
            _do_write();
            json->state = JSON_STATE_END;
            break;
        case JSON_STATE_ARRAY_ITEM:
            json_write(json, ",");
        case JSON_STATE_ARRAY:
            _do_write();
            json->state = JSON_STATE_ARRAY_ITEM;
            break;
        case JSON_STATE_OBJECT_KEY:
            _do_write();
            json->state = JSON_STATE_OBJECT_VALUE;
            break;
        default:
            ERROR("Unexpected float");
            DEBUG_STATE(json);
            json->state = JSON_STATE_ERROR;
    }
}

void json_string(json_stream *json, const char *x) {
    if (json->error)
        json->state = JSON_STATE_ERROR;
    if (json->state == JSON_STATE_ERROR)
        return;

    void _do_write() {
        // TODO: escape string
        json_write(json, "\"%s\"", x);
    }

    switch (json->state) {
        case JSON_STATE_START:
            _do_write();
            json->state = JSON_STATE_END;
            break;
        case JSON_STATE_ARRAY_ITEM:
            json_write(json, ",");
        case JSON_STATE_ARRAY:
            _do_write();
            json->state = JSON_STATE_ARRAY_ITEM;
            break;
        case JSON_STATE_OBJECT_VALUE:
            json_write(json, ",");
        case JSON_STATE_OBJECT:
            _do_write();
            json_write(json, ":");
            json->state = JSON_STATE_OBJECT_KEY;
            break;
        case JSON_STATE_OBJECT_KEY:
            _do_write();
            json->state = JSON_STATE_OBJECT_VALUE;
            break;
        default:
            ERROR("Unexpected string");
            DEBUG_STATE(json);
            json->state = JSON_STATE_ERROR;
    }
}

void json_boolean(json_stream *json, bool x) {
    if (json->error)
        json->state = JSON_STATE_ERROR;
    if (json->state == JSON_STATE_ERROR)
        return;

    void _do_write() {
        json_write(json, (x) ? "true" : "false");
    }

    switch (json->state) {
        case JSON_STATE_START:
            _do_write();
            json->state = JSON_STATE_END;
            break;
        case JSON_STATE_ARRAY_ITEM:
            json_write(json, ",");
        case JSON_STATE_ARRAY:
            _do_write();
            json->state = JSON_STATE_ARRAY_ITEM;
            break;
        case JSON_STATE_OBJECT_KEY:
            _do_write();
            json->state = JSON_STATE_OBJECT_VALUE;
            break;
        default:
            ERROR("Unexpected boolean");
            DEBUG_STATE(json);
            json->state = JSON_STATE_ERROR;
    }
}

void json_null(json_stream *json) {
    if (json->error)
        json->state = JSON_STATE_ERROR;
    if (json->state == JSON_STATE_ERROR)
        return;

    void _do_write() {
        json_write(json, "null");
    }

    switch (json->state) {
        case JSON_STATE_START:
            _do_write();
            json->state = JSON_STATE_END;
            break;
        case JSON_STATE_ARRAY_ITEM:
            json_write(json, ",");
        case JSON_STATE_ARRAY:
            _do_write();
            json->state = JSON_STATE_ARRAY_ITEM;
            break;
        case JSON_STATE_OBJECT_KEY:
            _do_write();
            json->state = JSON_STATE_OBJECT_VALUE;
            break;
        default:
            ERROR("Unexpected null");
            DEBUG_STATE(json);
            json->state = JSON_STATE_ERROR;
    }
}

