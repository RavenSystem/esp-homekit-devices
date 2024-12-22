#include <stdint.h>
#include <stdio.h>
#include <string.h>
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

void json_buffer_free(json_stream *json) {
    free(json->buffer);
    free(json);
}

void json_flush(json_stream *json) {
    if (!json->pos || json->error) {
        return;
    }
    
    if (json->on_flush) {
        if (json->on_flush(json->buffer, json->pos, json->context) < 0) {
            json->state = JSON_STATE_ERROR;
            json->error = true;
        }
    }
    
    json->pos = 0;
}

void json_put(json_stream *json, char c) {
    json->buffer[json->pos++] = c;
    if (json->pos >= HOMEKIT_JSON_BUFFER_SIZE - 1) {
        json_flush(json);
    }
}

void json_write(json_stream *json, const char *data, size_t size) {
    while (size) {
        size_t chunk_size = size;
        if (size > HOMEKIT_JSON_BUFFER_SIZE - json->pos) {
            chunk_size = HOMEKIT_JSON_BUFFER_SIZE - json->pos;
        }
        
        memcpy((char*) json->buffer + json->pos, data, chunk_size);
        
        json->pos += chunk_size;
        if (json->pos >= HOMEKIT_JSON_BUFFER_SIZE - 1) {
            json_flush(json);
        }
        
        data += chunk_size;
        size -= chunk_size;
    }
}

void json_object_start(json_stream *json) {
    if (json->error)
        json->state = JSON_STATE_ERROR;
    if (json->state == JSON_STATE_ERROR)
        return;

    switch (json->state) {
        case JSON_STATE_ARRAY_ITEM:
            json_put(json, ',');
        case JSON_STATE_START:
        case JSON_STATE_OBJECT_KEY:
        case JSON_STATE_ARRAY:
            json_put(json, '{');

            json->state = JSON_STATE_OBJECT;
            json->nesting[json->nesting_idx++] = JSON_NESTING_OBJECT;
            break;
        default:
            ERROR("Object start");
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
            json_put(json, '}');

            json->nesting_idx--;
            if (!json->nesting_idx) {
                json->state = JSON_STATE_END;
            } else {
                switch (json->nesting[json->nesting_idx - 1]) {
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
            ERROR("Object end");
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
            json_put(json, ',');
        case JSON_STATE_START:
        case JSON_STATE_OBJECT_KEY:
        case JSON_STATE_ARRAY:
            json_put(json, '[');

            json->state = JSON_STATE_ARRAY;
            json->nesting[json->nesting_idx++] = JSON_NESTING_ARRAY;
            break;
        default:
            ERROR("Array start");
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
            json_put(json, ']');

            json->nesting_idx--;
            if (!json->nesting_idx) {
                json->state = JSON_STATE_END;
            } else {
                switch (json->nesting[json->nesting_idx - 1]) {
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
            ERROR("Array end");
            DEBUG_STATE(json);
            json->state = JSON_STATE_ERROR;
    }
}

void json_integer(json_stream *json, long int x) {
    if (json->error)
        json->state = JSON_STATE_ERROR;
    if (json->state == JSON_STATE_ERROR)
        return;

    void _do_write() {
        size_t len = snprintf(NULL, 0, "%ld", x);
        char *buffer = malloc(len + 1);
        if (buffer) {
            snprintf(buffer, len + 1, "%ld", x);
            
            json_write(json, buffer, len);
            free(buffer);
        }
    }

    switch (json->state) {
        case JSON_STATE_START:
            _do_write();
            json->state = JSON_STATE_END;
            break;
        case JSON_STATE_ARRAY_ITEM:
            json_put(json, ',');
        case JSON_STATE_ARRAY:
            _do_write();
            json->state = JSON_STATE_ARRAY_ITEM;
            break;
        case JSON_STATE_OBJECT_KEY:
            _do_write();
            json->state = JSON_STATE_OBJECT_VALUE;
            break;
        default:
            ERROR("Integer");
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
        //snprintf(NULL, 0, "%1.15g", x);
        size_t len = snprintf(NULL, 0, "%1.7g", x);
        char *buffer = malloc(len + 1);
        if (buffer) {
            char *buffer = malloc(len + 1);
            snprintf(buffer, len + 1, "%1.7g", x);
            
            json_write(json, buffer, len);
            free(buffer);
        }
    }

    switch (json->state) {
        case JSON_STATE_START:
            _do_write();
            json->state = JSON_STATE_END;
            break;
        case JSON_STATE_ARRAY_ITEM:
            json_put(json, ',');
        case JSON_STATE_ARRAY:
            _do_write();
            json->state = JSON_STATE_ARRAY_ITEM;
            break;
        case JSON_STATE_OBJECT_KEY:
            _do_write();
            json->state = JSON_STATE_OBJECT_VALUE;
            break;
        default:
            ERROR("Float");
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
        json_put(json, '"');
        json_write(json, x, strlen(x));
        json_put(json, '"');
    }

    switch (json->state) {
        case JSON_STATE_START:
            _do_write();
            json->state = JSON_STATE_END;
            break;
        case JSON_STATE_ARRAY_ITEM:
            json_put(json, ',');
        case JSON_STATE_ARRAY:
            _do_write();
            json->state = JSON_STATE_ARRAY_ITEM;
            break;
        case JSON_STATE_OBJECT_VALUE:
            json_put(json, ',');
        case JSON_STATE_OBJECT:
            _do_write();
            json_put(json, ':');
            json->state = JSON_STATE_OBJECT_KEY;
            break;
        case JSON_STATE_OBJECT_KEY:
            _do_write();
            json->state = JSON_STATE_OBJECT_VALUE;
            break;
        default:
            ERROR("String");
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
        if (x) {
            json_write(json, "true", 4);
        } else {
            json_write(json, "false", 5);
        }
    }

    switch (json->state) {
        case JSON_STATE_START:
            _do_write();
            json->state = JSON_STATE_END;
            break;
        case JSON_STATE_ARRAY_ITEM:
            json_put(json, ',');
        case JSON_STATE_ARRAY:
            _do_write();
            json->state = JSON_STATE_ARRAY_ITEM;
            break;
        case JSON_STATE_OBJECT_KEY:
            _do_write();
            json->state = JSON_STATE_OBJECT_VALUE;
            break;
        default:
            ERROR("Boolean");
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
        json_write(json, "null", 4);
    }

    switch (json->state) {
        case JSON_STATE_START:
            _do_write();
            json->state = JSON_STATE_END;
            break;
        case JSON_STATE_ARRAY_ITEM:
            json_put(json, ',');
        case JSON_STATE_ARRAY:
            _do_write();
            json->state = JSON_STATE_ARRAY_ITEM;
            break;
        case JSON_STATE_OBJECT_KEY:
            _do_write();
            json->state = JSON_STATE_OBJECT_VALUE;
            break;
        default:
            ERROR("Null");
            DEBUG_STATE(json);
            json->state = JSON_STATE_ERROR;
    }
}

