#include <stdlib.h>
#include <string.h>

#include "tlv.h"


tlv_values_t *tlv_new() {
    tlv_values_t *values = malloc(sizeof(tlv_values_t));
    values->head = NULL;
    return values;
}


void tlv_free(tlv_values_t *values) {
    tlv_t *t = values->head;
    while (t) {
        tlv_t *t2 = t;
        t = t->next;
        if (t2->value)
            free(t2->value);
        free(t2);
    }
    free(values);
}


int tlv_add_value(tlv_values_t *values, byte type, const byte *value, size_t size) {
    tlv_t *tlv = malloc(sizeof(tlv_t));
    tlv->type = type;
    tlv->size = size;
    if (size) {
        tlv->value = malloc(size);
    } else {
        tlv->value = NULL;
    }
    memcpy(tlv->value, value, size);

    tlv->next = values->head;
    values->head = tlv;

    return 0;
}

int tlv_add_string_value(tlv_values_t *values, byte type, const char *value) {
    return tlv_add_value(values, type, (const byte *)value, strlen(value));
}

int tlv_add_integer_value(tlv_values_t *values, byte type, int value) {
    byte data[sizeof(value)];
    size_t size = 1;

    data[0] = value & 0xff;
    value >>= 8;

    while (value) {
        data[size++] = value & 0xff;
        value >>= 8;
    }

    return tlv_add_value(values, type, data, size);
}


tlv_t *tlv_get_value(const tlv_values_t *values, byte type) {
    tlv_t *t = values->head;
    while (t) {
        if (t->type == type)
            return t;
        t = t->next;
    }
    return NULL;
}


int tlv_get_integer_value(const tlv_values_t *values, byte type, int def) {
    tlv_t *t = tlv_get_value(values, type);
    if (!t)
        return 0;

    int x = 0;
    for (int i=t->size-1; i>=0; i--) {
        x = (x << 8) + t->value[i];
    }
    return x;
}


int tlv_format(const tlv_values_t *values, byte *buffer, size_t *size) {
    size_t required_size = 0;
    tlv_t *t = values->head;
    while (t) {
        required_size += t->size + 2 * ((t->size + 254) / 255);
        t = t->next;
    }

    if (*size < required_size) {
        *size = required_size;
        return -1;
    }

    *size = required_size;

    t = values->head;
    while (t) {
        byte *data = t->value;
        if (!t->size) {
            buffer[0] = t->type;
            buffer[1] = 0;
            buffer += 2;
            t = t->next;
            continue;
        }

        size_t remaining = t->size;

        while (remaining) {
            buffer[0] = t->type;
            size_t chunk_size = (remaining > 255) ? 255 : remaining;
            buffer[1] = chunk_size;
            memcpy(&buffer[2], data, chunk_size);
            remaining -= chunk_size;
            buffer += chunk_size + 2;
            data += chunk_size;
        }

        t = t->next;
    }

    return 0;
}


int tlv_parse(const byte *buffer, size_t length, tlv_values_t *values) {
    size_t i = 0;
    while (i < length) {
        tlv_t *t = malloc(sizeof(tlv_t));
        t->type = buffer[i];
        t->size = 0;

        size_t j = i;
        while (j < length && buffer[j] == t->type && buffer[j+1] == 255) {
            size_t chunk_size = buffer[j+1];
            t->size += chunk_size;
            j += chunk_size + 2;
        }
        if (j < length && buffer[j] == t->type) {
            t->size += buffer[j+1];
        }
        if (t->size == 0) {
            t->value = NULL;
        } else {
            t->value = malloc(t->size);

            byte *data = t->value;

            size_t remaining = t->size;
            while (remaining) {
                size_t chunk_size = buffer[i+1];
                memcpy(data, &buffer[i+2], chunk_size);
                data += chunk_size;
                i += chunk_size + 2;
                remaining -= chunk_size;
            }
        }

        t->next = values->head;
        values->head = t;
    }

    return 0;
}
