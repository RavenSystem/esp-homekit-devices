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

#include <string.h>
#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <limits.h>
#include <ctype.h>

#include "cJSON_rsf.h"

char* cJSON_rsf_GetStringValue(cJSON_rsf *item) {
    if (!cJSON_rsf_IsString(item)) {
        return NULL;
    }

    return item->valuestring;
}

/* Case insensitive string comparison, doesn't consider two NULL pointers equal though */
static int case_insensitive_strcmp(const unsigned char *string1, const unsigned char *string2)
{
    if ((string1 == NULL) || (string2 == NULL))
    {
        return 1;
    }

    if (string1 == string2)
    {
        return 0;
    }

    for(; tolower(*string1) == tolower(*string2); (void)string1++, string2++)
    {
        if (*string1 == '\0')
        {
            return 0;
        }
    }

    return tolower(*string1) - tolower(*string2);
}

static unsigned char* cJSON_rsf_strdup(const unsigned char* string)
{
    size_t length = 0;
    unsigned char *copy = NULL;

    if (string == NULL)
    {
        return NULL;
    }

    length = strlen((const char*)string) + sizeof("");
    copy = malloc(length);
    if (copy == NULL)
    {
        return NULL;
    }
    memcpy(copy, string, length);

    return copy;
}

/* Internal constructor. */
static cJSON_rsf *cJSON_rsf_New_Item()
{
    cJSON_rsf* node = malloc(sizeof(cJSON_rsf));
    if (node)
    {
        memset(node, 0, sizeof(cJSON_rsf));
    }

    return node;
}

/* Delete a cJSON_rsf structure. */
void cJSON_rsf_Delete(cJSON_rsf *item)
{
    cJSON_rsf *next = NULL;
    while (item != NULL)
    {
        next = item->next;
        if (!(item->type & cJSON_rsf_IsReference) && (item->child != NULL))
        {
            cJSON_rsf_Delete(item->child);
        }
        if (!(item->type & cJSON_rsf_IsReference) && (cJSON_rsf_IsString(item) || cJSON_rsf_IsRaw(item)) && (item->valuestring != NULL))
        {
            free(item->valuestring);
        }
        if (!(item->type & cJSON_rsf_StringIsConst) && (item->string != NULL))
        {
            free(item->string);
        }
        free(item);
        item = next;
    }
}

/* get the decimal point character of the current locale */
/*
static unsigned char get_decimal_point(void)
{
    return '.';
}
*/

typedef struct
{
    const unsigned char *content;
    size_t length;
    size_t offset;
    size_t depth;   /* How deeply nested (in arrays/objects) is the input at the current offset. */
} parse_buffer;

/* check if the given size is left to read in a given parse buffer (starting with 1) */
#define can_read(buffer, size) ((buffer != NULL) && (((buffer)->offset + size) <= (buffer)->length))
/* check if the buffer can be accessed at the given index (starting with 0) */
#define can_access_at_index(buffer, index) ((buffer != NULL) && (((buffer)->offset + index) < (buffer)->length))
#define cannot_access_at_index(buffer, index) (!can_access_at_index(buffer, index))
/* get a pointer to the buffer at the position */
#define buffer_at_offset(buffer) ((buffer)->content + (buffer)->offset)

/* Parse the input text to generate a number, and populate the result into item. */
static bool parse_number(cJSON_rsf * const item, parse_buffer * const input_buffer)
{
    float number = 0;
    unsigned char *after_end = NULL;
    unsigned char number_c_string[64];
    //unsigned char decimal_point = get_decimal_point();
    unsigned char decimal_point = '.';
    size_t i = 0;

    if ((input_buffer == NULL) || (input_buffer->content == NULL))
    {
        return false;
    }

    /* copy the number into a temporary buffer and replace '.' with the decimal point
     * of the current locale (for strtof)
     * This also takes care of '\0' not necessarily being available for marking the end of the input */
    for (i = 0; (i < (sizeof(number_c_string) - 1)) && can_access_at_index(input_buffer, i); i++)
    {
        switch (buffer_at_offset(input_buffer)[i])
        {
            case '0':
            case '1':
            case '2':
            case '3':
            case '4':
            case '5':
            case '6':
            case '7':
            case '8':
            case '9':
            case '+':
            case '-':
            case 'e':
            case 'E':
                number_c_string[i] = buffer_at_offset(input_buffer)[i];
                break;

            case '.':
                number_c_string[i] = decimal_point;
                break;

            default:
                goto loop_end;
        }
    }
loop_end:
    number_c_string[i] = '\0';

    number = strtof((const char*)number_c_string, (char**)&after_end);
    if (number_c_string == after_end)
    {
        return false; /* parse_error */
    }

    item->valuefloat = number;
    item->type = cJSON_rsf_Number;

    input_buffer->offset += (size_t)(after_end - number_c_string);
    return true;
}

/* don't ask me, but the original cJSON_rsf_SetNumberValue returns an integer or double */
float cJSON_rsf_SetNumberHelper(cJSON_rsf *object, float number)
{
    return object->valuefloat = number;
}

typedef struct
{
    unsigned char *buffer;
    size_t length;
    size_t offset;
    size_t depth;       /* current nesting depth (for formatted printing) */
    bool noalloc;
    bool format;  /* is this print a formatted print */
} printbuffer;

/* realloc printbuffer if necessary to have at least "needed" bytes more */
static unsigned char* ensure(printbuffer * const p, size_t needed)
{
    unsigned char *newbuffer = NULL;
    size_t newsize = 0;

    if ((p == NULL) || (p->buffer == NULL))
    {
        return NULL;
    }

    if ((p->length > 0) && (p->offset >= p->length))
    {
        /* make sure that offset is valid */
        return NULL;
    }

    if (needed > INT_MAX)
    {
        /* sizes bigger than INT_MAX are currently not supported */
        return NULL;
    }

    needed += p->offset + 1;
    if (needed <= p->length)
    {
        return p->buffer + p->offset;
    }

    if (p->noalloc) {
        return NULL;
    }

    /* calculate new buffer size */
    if (needed > (INT_MAX / 2))
    {
        /* overflow of int, use INT_MAX if possible */
        if (needed <= INT_MAX)
        {
            newsize = INT_MAX;
        }
        else
        {
            return NULL;
        }
    }
    else
    {
        newsize = needed * 2;
    }
    
    /* reallocate with realloc if available */
    newbuffer = realloc(p->buffer, newsize);
    if (newbuffer == NULL)
    {
        free(p->buffer);
        p->length = 0;
        p->buffer = NULL;

        return NULL;
    }
    
    p->length = newsize;
    p->buffer = newbuffer;

    return newbuffer + p->offset;
}

/* calculate the new length of the string in a printbuffer and update the offset */
static void update_offset(printbuffer * const buffer)
{
    const unsigned char *buffer_pointer = NULL;
    if ((buffer == NULL) || (buffer->buffer == NULL))
    {
        return;
    }
    buffer_pointer = buffer->buffer + buffer->offset;

    buffer->offset += strlen((const char*)buffer_pointer);
}

/* Render the number nicely from the given item into a string. */
static bool print_number(const cJSON_rsf * const item, printbuffer * const output_buffer)
{
    unsigned char *output_pointer = NULL;
    float d = item->valuefloat;
    int length = 0;
    size_t i = 0;
    unsigned char number_buffer[26]; /* temporary buffer to print the number into */
    //unsigned char decimal_point = get_decimal_point();
    unsigned char decimal_point = '.';
    float test;

    if (output_buffer == NULL)
    {
        return false;
    }

    /* This checks for NaN and Infinity */
    if ((d * 0) != 0)
    {
        length = sprintf((char*)number_buffer, "null");
    }
    else
    {
        /* Try 15 decimal places of precision to avoid nonsignificant nonzero digits */
        //length = sprintf((char*)number_buffer, "%1.15g", d);
        length = sprintf((char*)number_buffer, "%1.7g", d);

        /* Check whether the original float can be recovered */
        //if ((sscanf((char*)number_buffer, "%lg", &test) != 1) || ((double)test != d))
        if ((sscanf((char*)number_buffer, "%g", &test) != 1) || ((float)test != d))
        {
            /* If not, print with 17 decimal places of precision */
            //length = sprintf((char*)number_buffer, "%1.17g", d);
            length = sprintf((char*)number_buffer, "%1.9g", d);
        }
    }

    /* sprintf failed or buffer overrun occured */
    if ((length < 0) || (length > (int)(sizeof(number_buffer) - 1)))
    {
        return false;
    }

    /* reserve appropriate space in the output */
    output_pointer = ensure(output_buffer, (size_t)length + sizeof(""));
    if (output_pointer == NULL)
    {
        return false;
    }

    /* copy the printed number to the output and replace locale
     * dependent decimal point with '.' */
    for (i = 0; i < ((size_t)length); i++)
    {
        if (number_buffer[i] == decimal_point)
        {
            output_pointer[i] = '.';
            continue;
        }

        output_pointer[i] = number_buffer[i];
    }
    output_pointer[i] = '\0';

    output_buffer->offset += (size_t)length;

    return true;
}

/* parse 4 digit hexadecimal number */
static unsigned parse_hex4(const unsigned char * const input)
{
    unsigned int h = 0;
    size_t i = 0;

    for (i = 0; i < 4; i++)
    {
        /* parse digit */
        if ((input[i] >= '0') && (input[i] <= '9'))
        {
            h += (unsigned int) input[i] - '0';
        }
        else if ((input[i] >= 'A') && (input[i] <= 'F'))
        {
            h += (unsigned int) 10 + input[i] - 'A';
        }
        else if ((input[i] >= 'a') && (input[i] <= 'f'))
        {
            h += (unsigned int) 10 + input[i] - 'a';
        }
        else /* invalid */
        {
            return 0;
        }

        if (i < 3)
        {
            /* shift left to make place for the next nibble */
            h = h << 4;
        }
    }

    return h;
}

/* converts a UTF-16 literal to UTF-8
 * A literal can be one or two sequences of the form \uXXXX */
static unsigned char utf16_literal_to_utf8(const unsigned char * const input_pointer, const unsigned char * const input_end, unsigned char **output_pointer)
{
    long unsigned int codepoint = 0;
    unsigned int first_code = 0;
    const unsigned char *first_sequence = input_pointer;
    unsigned char utf8_length = 0;
    unsigned char utf8_position = 0;
    unsigned char sequence_length = 0;
    unsigned char first_byte_mark = 0;

    if ((input_end - first_sequence) < 6)
    {
        /* input ends unexpectedly */
        goto fail;
    }

    /* get the first utf16 sequence */
    first_code = parse_hex4(first_sequence + 2);

    /* check that the code is valid */
    if (((first_code >= 0xDC00) && (first_code <= 0xDFFF)))
    {
        goto fail;
    }

    /* UTF16 surrogate pair */
    if ((first_code >= 0xD800) && (first_code <= 0xDBFF))
    {
        const unsigned char *second_sequence = first_sequence + 6;
        unsigned int second_code = 0;
        sequence_length = 12; /* \uXXXX\uXXXX */

        if ((input_end - second_sequence) < 6)
        {
            /* input ends unexpectedly */
            goto fail;
        }

        if ((second_sequence[0] != '\\') || (second_sequence[1] != 'u'))
        {
            /* missing second half of the surrogate pair */
            goto fail;
        }

        /* get the second utf16 sequence */
        second_code = parse_hex4(second_sequence + 2);
        /* check that the code is valid */
        if ((second_code < 0xDC00) || (second_code > 0xDFFF))
        {
            /* invalid second half of the surrogate pair */
            goto fail;
        }


        /* calculate the unicode codepoint from the surrogate pair */
        codepoint = 0x10000 + (((first_code & 0x3FF) << 10) | (second_code & 0x3FF));
    }
    else
    {
        sequence_length = 6; /* \uXXXX */
        codepoint = first_code;
    }

    /* encode as UTF-8
     * takes at maximum 4 bytes to encode:
     * 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx */
    if (codepoint < 0x80)
    {
        /* normal ascii, encoding 0xxxxxxx */
        utf8_length = 1;
    }
    else if (codepoint < 0x800)
    {
        /* two bytes, encoding 110xxxxx 10xxxxxx */
        utf8_length = 2;
        first_byte_mark = 0xC0; /* 11000000 */
    }
    else if (codepoint < 0x10000)
    {
        /* three bytes, encoding 1110xxxx 10xxxxxx 10xxxxxx */
        utf8_length = 3;
        first_byte_mark = 0xE0; /* 11100000 */
    }
    else if (codepoint <= 0x10FFFF)
    {
        /* four bytes, encoding 1110xxxx 10xxxxxx 10xxxxxx 10xxxxxx */
        utf8_length = 4;
        first_byte_mark = 0xF0; /* 11110000 */
    }
    else
    {
        /* invalid unicode codepoint */
        goto fail;
    }

    /* encode as utf8 */
    for (utf8_position = (unsigned char)(utf8_length - 1); utf8_position > 0; utf8_position--)
    {
        /* 10xxxxxx */
        (*output_pointer)[utf8_position] = (unsigned char)((codepoint | 0x80) & 0xBF);
        codepoint >>= 6;
    }
    /* encode first byte */
    if (utf8_length > 1)
    {
        (*output_pointer)[0] = (unsigned char)((codepoint | first_byte_mark) & 0xFF);
    }
    else
    {
        (*output_pointer)[0] = (unsigned char)(codepoint & 0x7F);
    }

    *output_pointer += utf8_length;

    return sequence_length;

fail:
    return 0;
}

/* Parse the input text into an unescaped cinput, and populate item. */
static bool parse_string(cJSON_rsf * const item, parse_buffer * const input_buffer)
{
    const unsigned char *input_pointer = buffer_at_offset(input_buffer) + 1;
    const unsigned char *input_end = buffer_at_offset(input_buffer) + 1;
    unsigned char *output_pointer = NULL;
    unsigned char *output = NULL;

    /* not a string */
    if (buffer_at_offset(input_buffer)[0] != '\"')
    {
        goto fail;
    }

    {
        /* calculate approximate size of the output (overestimate) */
        size_t allocation_length = 0;
        size_t skipped_bytes = 0;
        while (((size_t)(input_end - input_buffer->content) < input_buffer->length) && (*input_end != '\"'))
        {
            /* is escape sequence */
            if (input_end[0] == '\\')
            {
                if ((size_t)(input_end + 1 - input_buffer->content) >= input_buffer->length)
                {
                    /* prevent buffer overflow when last input character is a backslash */
                    goto fail;
                }
                skipped_bytes++;
                input_end++;
            }
            input_end++;
        }
        if (((size_t)(input_end - input_buffer->content) >= input_buffer->length) || (*input_end != '\"'))
        {
            goto fail; /* string ended unexpectedly */
        }

        /* This is at most how much we need for the output */
        allocation_length = (size_t) (input_end - buffer_at_offset(input_buffer)) - skipped_bytes;
        output = malloc(allocation_length + sizeof(""));
        if (output == NULL)
        {
            goto fail; /* allocation failure */
        }
    }

    output_pointer = output;
    /* loop through the string literal */
    while (input_pointer < input_end)
    {
        if (*input_pointer != '\\')
        {
            *output_pointer++ = *input_pointer++;
        }
        /* escape sequence */
        else
        {
            unsigned char sequence_length = 2;
            if ((input_end - input_pointer) < 1)
            {
                goto fail;
            }

            switch (input_pointer[1])
            {
                case 'b':
                    *output_pointer++ = '\b';
                    break;
                case 'f':
                    *output_pointer++ = '\f';
                    break;
                case 'n':
                    *output_pointer++ = '\n';
                    break;
                case 'r':
                    *output_pointer++ = '\r';
                    break;
                case 't':
                    *output_pointer++ = '\t';
                    break;
                case '\"':
                case '\\':
                case '/':
                    *output_pointer++ = input_pointer[1];
                    break;

                /* UTF-16 literal */
                case 'u':
                    sequence_length = utf16_literal_to_utf8(input_pointer, input_end, &output_pointer);
                    if (sequence_length == 0)
                    {
                        /* failed to convert UTF16-literal to UTF-8 */
                        goto fail;
                    }
                    break;

                default:
                    goto fail;
            }
            input_pointer += sequence_length;
        }
    }

    /* zero terminate the output */
    *output_pointer = '\0';

    item->type = cJSON_rsf_String;
    item->valuestring = (char*)output;

    input_buffer->offset = (size_t) (input_end - input_buffer->content);
    input_buffer->offset++;

    return true;

fail:
    if (output != NULL)
    {
        free(output);
    }

    if (input_pointer != NULL)
    {
        input_buffer->offset = (size_t)(input_pointer - input_buffer->content);
    }

    return false;
}

/* Render the cstring provided to an escaped version that can be printed. */
static bool print_string_ptr(const unsigned char * const input, printbuffer * const output_buffer)
{
    const unsigned char *input_pointer = NULL;
    unsigned char *output = NULL;
    unsigned char *output_pointer = NULL;
    size_t output_length = 0;
    /* numbers of additional characters needed for escaping */
    size_t escape_characters = 0;

    if (output_buffer == NULL)
    {
        return false;
    }

    /* empty string */
    if (input == NULL)
    {
        output = ensure(output_buffer, sizeof("\"\""));
        if (output == NULL)
        {
            return false;
        }
        strcpy((char*)output, "\"\"");

        return true;
    }

    /* set "flag" to 1 if something needs to be escaped */
    for (input_pointer = input; *input_pointer; input_pointer++)
    {
        switch (*input_pointer)
        {
            case '\"':
            case '\\':
            case '\b':
            case '\f':
            case '\n':
            case '\r':
            case '\t':
                /* one character escape sequence */
                escape_characters++;
                break;
            default:
                if (*input_pointer < 32)
                {
                    /* UTF-16 escape sequence uXXXX */
                    escape_characters += 5;
                }
                break;
        }
    }
    output_length = (size_t)(input_pointer - input) + escape_characters;

    output = ensure(output_buffer, output_length + sizeof("\"\""));
    if (output == NULL)
    {
        return false;
    }

    /* no characters have to be escaped */
    if (escape_characters == 0)
    {
        output[0] = '\"';
        memcpy(output + 1, input, output_length);
        output[output_length + 1] = '\"';
        output[output_length + 2] = '\0';

        return true;
    }

    output[0] = '\"';
    output_pointer = output + 1;
    /* copy the string */
    for (input_pointer = input; *input_pointer != '\0'; (void)input_pointer++, output_pointer++)
    {
        if ((*input_pointer > 31) && (*input_pointer != '\"') && (*input_pointer != '\\'))
        {
            /* normal character, copy */
            *output_pointer = *input_pointer;
        }
        else
        {
            /* character needs to be escaped */
            *output_pointer++ = '\\';
            switch (*input_pointer)
            {
                case '\\':
                    *output_pointer = '\\';
                    break;
                case '\"':
                    *output_pointer = '\"';
                    break;
                case '\b':
                    *output_pointer = 'b';
                    break;
                case '\f':
                    *output_pointer = 'f';
                    break;
                case '\n':
                    *output_pointer = 'n';
                    break;
                case '\r':
                    *output_pointer = 'r';
                    break;
                case '\t':
                    *output_pointer = 't';
                    break;
                default:
                    /* escape and print as unicode codepoint */
                    sprintf((char*)output_pointer, "u%04x", *input_pointer);
                    output_pointer += 4;
                    break;
            }
        }
    }
    output[output_length + 1] = '\"';
    output[output_length + 2] = '\0';

    return true;
}

/* Invoke print_string_ptr (which is useful) on an item. */
static bool print_string(const cJSON_rsf * const item, printbuffer * const p)
{
    return print_string_ptr((unsigned char*)item->valuestring, p);
}

/* Predeclare these prototypes. */
static bool parse_value(cJSON_rsf * const item, parse_buffer * const input_buffer);
static bool print_value(const cJSON_rsf * const item, printbuffer * const output_buffer);
static bool parse_array(cJSON_rsf * const item, parse_buffer * const input_buffer);
static bool print_array(const cJSON_rsf * const item, printbuffer * const output_buffer);
static bool parse_object(cJSON_rsf * const item, parse_buffer * const input_buffer);
static bool print_object(const cJSON_rsf * const item, printbuffer * const output_buffer);

/* Utility to jump whitespace and cr/lf */
static parse_buffer *buffer_skip_whitespace(parse_buffer * const buffer)
{
    if ((buffer == NULL) || (buffer->content == NULL))
    {
        return NULL;
    }

    while (can_access_at_index(buffer, 0) && (buffer_at_offset(buffer)[0] <= 32))
    {
       buffer->offset++;
    }

    if (buffer->offset == buffer->length)
    {
        buffer->offset--;
    }

    return buffer;
}

/* skip the UTF-8 BOM (byte order mark) if it is at the beginning of a buffer */
static parse_buffer *skip_utf8_bom(parse_buffer * const buffer)
{
    if ((buffer == NULL) || (buffer->content == NULL) || (buffer->offset != 0))
    {
        return NULL;
    }

    if (can_access_at_index(buffer, 4) && (strncmp((const char*)buffer_at_offset(buffer), "\xEF\xBB\xBF", 3) == 0))
    {
        buffer->offset += 3;
    }

    return buffer;
}

/* Parse an object - create a new root, and populate. */
cJSON_rsf* cJSON_rsf_ParseWithOpts(const char *value, bool require_null_terminated)
{
    parse_buffer buffer = { 0, 0, 0, 0 };
    cJSON_rsf *item = NULL;

    if (value == NULL)
    {
        goto fail;
    }

    buffer.content = (const unsigned char*)value;
    buffer.length = strlen((const char*)value) + sizeof("");
    buffer.offset = 0;

    item = cJSON_rsf_New_Item();
    if (item == NULL) /* memory fail */
    {
        goto fail;
    }

    if (!parse_value(item, buffer_skip_whitespace(skip_utf8_bom(&buffer))))
    {
        /* parse failure. ep is set. */
        goto fail;
    }

    /* if we require null-terminated JSON without appended garbage, skip and then check for a null terminator */
    if (require_null_terminated)
    {
        buffer_skip_whitespace(&buffer);
        if ((buffer.offset >= buffer.length) || buffer_at_offset(&buffer)[0] != '\0')
        {
            goto fail;
        }
    }

    return item;

fail:
    if (item != NULL)
    {
        cJSON_rsf_Delete(item);
    }

    return NULL;
}

/* Default options for cJSON_rsf_Parse */
cJSON_rsf* cJSON_rsf_Parse(const char *value)
{
    parse_buffer buffer = { 0, 0, 0, 0 };
    cJSON_rsf *item = NULL;

    if (value == NULL)
    {
        goto fail;
    }

    buffer.content = (const unsigned char*) value;
    buffer.length = strlen((const char*) value) + sizeof("");
    buffer.offset = 0;

    item = cJSON_rsf_New_Item();
    if (item == NULL) /* memory fail */
    {
        goto fail;
    }

    if (!parse_value(item, buffer_skip_whitespace(skip_utf8_bom(&buffer))))
    {
        /* parse failure. ep is set. */
        goto fail;
    }

    return item;

fail:
    if (item != NULL)
    {
        cJSON_rsf_Delete(item);
    }

    return NULL;
}

#define cjson_min(a, b) ((a < b) ? a : b)

static unsigned char *print(const cJSON_rsf * const item, bool format)
{
    static const size_t default_buffer_size = 256;
    printbuffer buffer[1];
    unsigned char *printed = NULL;

    memset(buffer, 0, sizeof(buffer));

    /* create buffer */
    buffer->buffer = malloc(default_buffer_size);
    buffer->length = default_buffer_size;
    buffer->format = format;
    if (buffer->buffer == NULL)
    {
        goto fail;
    }

    /* print the value */
    if (!print_value(item, buffer))
    {
        goto fail;
    }
    update_offset(buffer);

    printed = realloc(buffer->buffer, buffer->offset + 1);
    if (printed == NULL) {
        goto fail;
    }
    buffer->buffer = NULL;

    return printed;

fail:
    if (buffer->buffer != NULL)
    {
        free(buffer->buffer);
    }

    if (printed != NULL)
    {
        free(printed);
    }

    return NULL;
}

/* Render a cJSON_rsf item/entity/structure to text. */
char* cJSON_rsf_Print(const cJSON_rsf *item)
{
    return (char*)print(item, true);
}

char* cJSON_rsf_PrintUnformatted(const cJSON_rsf *item)
{
    return (char*)print(item, false);
}

char* cJSON_rsf_PrintBuffered(const cJSON_rsf *item, int prebuffer, bool fmt)
{
    printbuffer p = { 0, 0, 0, 0, 0, 0 };

    if (prebuffer < 0)
    {
        return NULL;
    }

    p.buffer = malloc((size_t) prebuffer);
    if (!p.buffer)
    {
        return NULL;
    }

    p.length = (size_t) prebuffer;
    p.offset = 0;
    p.noalloc = false;
    p.format = fmt;

    if (!print_value(item, &p))
    {
        free(p.buffer);
        return NULL;
    }
    
    return (char*)p.buffer;
}

bool cJSON_rsf_PrintPreallocated(cJSON_rsf *item, char *buf, const int len, const bool fmt)
{
    printbuffer p = { 0, 0, 0, 0, 0, 0 };

    if ((len < 0) || (buf == NULL))
    {
        return false;
    }

    p.buffer = (unsigned char*)buf;
    p.length = (size_t)len;
    p.offset = 0;
    p.noalloc = true;
    p.format = fmt;

    return print_value(item, &p);
}

/* Parser core - when encountering text, process appropriately. */
static bool parse_value(cJSON_rsf * const item, parse_buffer * const input_buffer)
{
    if ((input_buffer == NULL) || (input_buffer->content == NULL))
    {
        return false; /* no input */
    }

    /* parse the different types of values */
    /* null */
    if (can_read(input_buffer, 4) && (strncmp((const char*)buffer_at_offset(input_buffer), "null", 4) == 0))
    {
        item->type = cJSON_rsf_NULL;
        input_buffer->offset += 4;
        return true;
    }
    /* false */
    if (can_read(input_buffer, 5) && (strncmp((const char*)buffer_at_offset(input_buffer), "false", 5) == 0))
    {
        item->type = cJSON_rsf_False;
        input_buffer->offset += 5;
        return true;
    }
    /* true */
    if (can_read(input_buffer, 4) && (strncmp((const char*)buffer_at_offset(input_buffer), "true", 4) == 0))
    {
        item->type = cJSON_rsf_True;
        input_buffer->offset += 4;
        return true;
    }
    /* string */
    if (can_access_at_index(input_buffer, 0) && (buffer_at_offset(input_buffer)[0] == '\"'))
    {
        return parse_string(item, input_buffer);
    }
    /* number */
    if (can_access_at_index(input_buffer, 0) && ((buffer_at_offset(input_buffer)[0] == '-') || ((buffer_at_offset(input_buffer)[0] >= '0') && (buffer_at_offset(input_buffer)[0] <= '9'))))
    {
        return parse_number(item, input_buffer);
    }
    /* array */
    if (can_access_at_index(input_buffer, 0) && (buffer_at_offset(input_buffer)[0] == '['))
    {
        return parse_array(item, input_buffer);
    }
    /* object */
    if (can_access_at_index(input_buffer, 0) && (buffer_at_offset(input_buffer)[0] == '{'))
    {
        return parse_object(item, input_buffer);
    }

    return false;
}

/* Render a value to text. */
static bool print_value(const cJSON_rsf * const item, printbuffer * const output_buffer)
{
    unsigned char *output = NULL;

    if ((item == NULL) || (output_buffer == NULL))
    {
        return false;
    }

    switch ((item->type) & 0xFF)
    {
        case cJSON_rsf_NULL:
            output = ensure(output_buffer, 5);
            if (output == NULL)
            {
                return false;
            }
            strcpy((char*)output, "null");
            return true;

        case cJSON_rsf_False:
            output = ensure(output_buffer, 6);
            if (output == NULL)
            {
                return false;
            }
            strcpy((char*)output, "false");
            return true;

        case cJSON_rsf_True:
            output = ensure(output_buffer, 5);
            if (output == NULL)
            {
                return false;
            }
            strcpy((char*)output, "true");
            return true;

        case cJSON_rsf_Number:
            return print_number(item, output_buffer);

        case cJSON_rsf_Raw:
        {
            size_t raw_length = 0;
            if (item->valuestring == NULL)
            {
                return false;
            }

            raw_length = strlen(item->valuestring) + sizeof("");
            output = ensure(output_buffer, raw_length);
            if (output == NULL)
            {
                return false;
            }
            memcpy(output, item->valuestring, raw_length);
            return true;
        }

        case cJSON_rsf_String:
            return print_string(item, output_buffer);

        case cJSON_rsf_Array:
            return print_array(item, output_buffer);

        case cJSON_rsf_Object:
            return print_object(item, output_buffer);

        default:
            return false;
    }
}

/* Build an array from input text. */
static bool parse_array(cJSON_rsf * const item, parse_buffer * const input_buffer)
{
    cJSON_rsf *head = NULL; /* head of the linked list */
    cJSON_rsf *current_item = NULL;

    if (input_buffer->depth >= CJSON_NESTING_LIMIT)
    {
        return false; /* to deeply nested */
    }
    input_buffer->depth++;

    if (buffer_at_offset(input_buffer)[0] != '[')
    {
        /* not an array */
        goto fail;
    }

    input_buffer->offset++;
    buffer_skip_whitespace(input_buffer);
    if (can_access_at_index(input_buffer, 0) && (buffer_at_offset(input_buffer)[0] == ']'))
    {
        /* empty array */
        goto success;
    }

    /* check if we skipped to the end of the buffer */
    if (cannot_access_at_index(input_buffer, 0))
    {
        input_buffer->offset--;
        goto fail;
    }

    /* step back to character in front of the first element */
    input_buffer->offset--;
    /* loop through the comma separated array elements */
    do
    {
        /* allocate next item */
        cJSON_rsf *new_item = cJSON_rsf_New_Item();
        if (new_item == NULL)
        {
            goto fail; /* allocation failure */
        }

        /* attach next item to list */
        if (head == NULL)
        {
            /* start the linked list */
            current_item = head = new_item;
        }
        else
        {
            /* add to the end and advance */
            current_item->next = new_item;
            new_item->prev = current_item;
            current_item = new_item;
        }

        /* parse next value */
        input_buffer->offset++;
        buffer_skip_whitespace(input_buffer);
        if (!parse_value(current_item, input_buffer))
        {
            goto fail; /* failed to parse value */
        }
        buffer_skip_whitespace(input_buffer);
    }
    while (can_access_at_index(input_buffer, 0) && (buffer_at_offset(input_buffer)[0] == ','));

    if (cannot_access_at_index(input_buffer, 0) || buffer_at_offset(input_buffer)[0] != ']')
    {
        goto fail; /* expected end of array */
    }

success:
    input_buffer->depth--;

    item->type = cJSON_rsf_Array;
    item->child = head;

    input_buffer->offset++;

    return true;

fail:
    if (head != NULL)
    {
        cJSON_rsf_Delete(head);
    }

    return false;
}

/* Render an array to text */
static bool print_array(const cJSON_rsf * const item, printbuffer * const output_buffer)
{
    unsigned char *output_pointer = NULL;
    size_t length = 0;
    cJSON_rsf *current_element = item->child;

    if (output_buffer == NULL)
    {
        return false;
    }

    /* Compose the output array. */
    /* opening square bracket */
    output_pointer = ensure(output_buffer, 1);
    if (output_pointer == NULL)
    {
        return false;
    }

    *output_pointer = '[';
    output_buffer->offset++;
    output_buffer->depth++;

    while (current_element != NULL)
    {
        if (!print_value(current_element, output_buffer))
        {
            return false;
        }
        update_offset(output_buffer);
        if (current_element->next)
        {
            length = (size_t) (output_buffer->format ? 2 : 1);
            output_pointer = ensure(output_buffer, length + 1);
            if (output_pointer == NULL)
            {
                return false;
            }
            *output_pointer++ = ',';
            if(output_buffer->format)
            {
                *output_pointer++ = ' ';
            }
            *output_pointer = '\0';
            output_buffer->offset += length;
        }
        current_element = current_element->next;
    }

    output_pointer = ensure(output_buffer, 2);
    if (output_pointer == NULL)
    {
        return false;
    }
    *output_pointer++ = ']';
    *output_pointer = '\0';
    output_buffer->depth--;

    return true;
}

/* Build an object from the text. */
static bool parse_object(cJSON_rsf * const item, parse_buffer * const input_buffer)
{
    cJSON_rsf *head = NULL; /* linked list head */
    cJSON_rsf *current_item = NULL;

    if (input_buffer->depth >= CJSON_NESTING_LIMIT)
    {
        return false; /* to deeply nested */
    }
    input_buffer->depth++;

    if (cannot_access_at_index(input_buffer, 0) || (buffer_at_offset(input_buffer)[0] != '{'))
    {
        goto fail; /* not an object */
    }

    input_buffer->offset++;
    buffer_skip_whitespace(input_buffer);
    if (can_access_at_index(input_buffer, 0) && (buffer_at_offset(input_buffer)[0] == '}'))
    {
        goto success; /* empty object */
    }

    /* check if we skipped to the end of the buffer */
    if (cannot_access_at_index(input_buffer, 0))
    {
        input_buffer->offset--;
        goto fail;
    }

    /* step back to character in front of the first element */
    input_buffer->offset--;
    /* loop through the comma separated array elements */
    do
    {
        /* allocate next item */
        cJSON_rsf *new_item = cJSON_rsf_New_Item();
        if (new_item == NULL)
        {
            goto fail; /* allocation failure */
        }

        /* attach next item to list */
        if (head == NULL)
        {
            /* start the linked list */
            current_item = head = new_item;
        }
        else
        {
            /* add to the end and advance */
            current_item->next = new_item;
            new_item->prev = current_item;
            current_item = new_item;
        }

        /* parse the name of the child */
        input_buffer->offset++;
        buffer_skip_whitespace(input_buffer);
        if (!parse_string(current_item, input_buffer))
        {
            goto fail; /* faile to parse name */
        }
        buffer_skip_whitespace(input_buffer);

        /* swap valuestring and string, because we parsed the name */
        current_item->string = current_item->valuestring;
        current_item->valuestring = NULL;

        if (cannot_access_at_index(input_buffer, 0) || (buffer_at_offset(input_buffer)[0] != ':'))
        {
            goto fail; /* invalid object */
        }

        /* parse the value */
        input_buffer->offset++;
        buffer_skip_whitespace(input_buffer);
        if (!parse_value(current_item, input_buffer))
        {
            goto fail; /* failed to parse value */
        }
        buffer_skip_whitespace(input_buffer);
    }
    while (can_access_at_index(input_buffer, 0) && (buffer_at_offset(input_buffer)[0] == ','));

    if (cannot_access_at_index(input_buffer, 0) || (buffer_at_offset(input_buffer)[0] != '}'))
    {
        goto fail; /* expected end of object */
    }

success:
    input_buffer->depth--;

    item->type = cJSON_rsf_Object;
    item->child = head;

    input_buffer->offset++;
    return true;

fail:
    if (head != NULL)
    {
        cJSON_rsf_Delete(head);
    }

    return false;
}

/* Render an object to text. */
static bool print_object(const cJSON_rsf * const item, printbuffer * const output_buffer)
{
    unsigned char *output_pointer = NULL;
    size_t length = 0;
    cJSON_rsf *current_item = item->child;

    if (output_buffer == NULL)
    {
        return false;
    }

    /* Compose the output: */
    length = (size_t) (output_buffer->format ? 2 : 1); /* fmt: {\n */
    output_pointer = ensure(output_buffer, length + 1);
    if (output_pointer == NULL)
    {
        return false;
    }

    *output_pointer++ = '{';
    output_buffer->depth++;
    if (output_buffer->format)
    {
        *output_pointer++ = '\n';
    }
    output_buffer->offset += length;

    while (current_item)
    {
        if (output_buffer->format)
        {
            size_t i;
            output_pointer = ensure(output_buffer, output_buffer->depth);
            if (output_pointer == NULL)
            {
                return false;
            }
            for (i = 0; i < output_buffer->depth; i++)
            {
                *output_pointer++ = '\t';
            }
            output_buffer->offset += output_buffer->depth;
        }

        /* print key */
        if (!print_string_ptr((unsigned char*)current_item->string, output_buffer))
        {
            return false;
        }
        update_offset(output_buffer);

        length = (size_t) (output_buffer->format ? 2 : 1);
        output_pointer = ensure(output_buffer, length);
        if (output_pointer == NULL)
        {
            return false;
        }
        *output_pointer++ = ':';
        if (output_buffer->format)
        {
            *output_pointer++ = '\t';
        }
        output_buffer->offset += length;

        /* print value */
        if (!print_value(current_item, output_buffer))
        {
            return false;
        }
        update_offset(output_buffer);

        /* print comma if not last */
        length = (size_t) ((output_buffer->format ? 1 : 0) + (current_item->next ? 1 : 0));
        output_pointer = ensure(output_buffer, length + 1);
        if (output_pointer == NULL)
        {
            return false;
        }
        if (current_item->next)
        {
            *output_pointer++ = ',';
        }

        if (output_buffer->format)
        {
            *output_pointer++ = '\n';
        }
        *output_pointer = '\0';
        output_buffer->offset += length;

        current_item = current_item->next;
    }

    output_pointer = ensure(output_buffer, output_buffer->format ? (output_buffer->depth + 1) : 2);
    if (output_pointer == NULL)
    {
        return false;
    }
    if (output_buffer->format)
    {
        size_t i;
        for (i = 0; i < (output_buffer->depth - 1); i++)
        {
            *output_pointer++ = '\t';
        }
    }
    *output_pointer++ = '}';
    *output_pointer = '\0';
    output_buffer->depth--;

    return true;
}

/* Get Array size/item / object item. */
size_t cJSON_rsf_GetArraySize(const cJSON_rsf *array)
{
    cJSON_rsf *child = NULL;
    size_t size = 0;

    if (array == NULL)
    {
        return 0;
    }

    child = array->child;

    while(child != NULL)
    {
        size++;
        child = child->next;
    }
    
    /* FIXME: Can overflow here. Cannot be fixed without breaking the API */

    return size;
}

static cJSON_rsf* get_array_item(const cJSON_rsf *array, size_t index)
{
    cJSON_rsf *current_child = NULL;

    if (array == NULL)
    {
        return NULL;
    }

    current_child = array->child;
    while ((current_child != NULL) && (index > 0))
    {
        index--;
        current_child = current_child->next;
    }

    return current_child;
}

cJSON_rsf* cJSON_rsf_GetArrayItem(const cJSON_rsf *array, int index)
{
    if (index < 0)
    {
        return NULL;
    }

    return get_array_item(array, (size_t)index);
}

static cJSON_rsf *get_object_item(const cJSON_rsf * const object, const char * const name, const bool case_sensitive)
{
    cJSON_rsf *current_element = NULL;

    if ((object == NULL) || (name == NULL))
    {
        return NULL;
    }

    current_element = object->child;
    if (case_sensitive)
    {
        while ((current_element != NULL) &&
               (current_element->string == NULL || (strcmp(name, current_element->string) != 0)))
        {
            current_element = current_element->next;
        }
    }
    else
    {
        while ((current_element != NULL) &&
               (current_element->string == NULL || (case_insensitive_strcmp((const unsigned char*)name, (const unsigned char*)(current_element->string)) != 0)))
        {
            current_element = current_element->next;
        }
    }

    return current_element;
}

cJSON_rsf* cJSON_rsf_GetObjectItem(const cJSON_rsf * const object, const char * const string)
{
    return get_object_item(object, string, false);
}

cJSON_rsf* cJSON_rsf_GetObjectItemCaseSensitive(const cJSON_rsf * const object, const char * const string)
{
    return get_object_item(object, string, true);
}

bool cJSON_rsf_HasObjectItem(const cJSON_rsf *object, const char *string)
{
    return cJSON_rsf_GetObjectItem(object, string) ? 1 : 0;
}

/* Utility for array list handling. */
static void suffix_object(cJSON_rsf *prev, cJSON_rsf *item)
{
    prev->next = item;
    item->prev = prev;
}

/* Utility for handling references. */
static cJSON_rsf *create_reference(const cJSON_rsf *item)
{
    cJSON_rsf *reference = NULL;
    if (item == NULL)
    {
        return NULL;
    }

    reference = cJSON_rsf_New_Item();
    if (reference == NULL)
    {
        return NULL;
    }

    memcpy(reference, item, sizeof(cJSON_rsf));
    reference->string = NULL;
    reference->type |= cJSON_rsf_IsReference;
    reference->next = reference->prev = NULL;
    return reference;
}

static bool add_item_to_array(cJSON_rsf *array, cJSON_rsf *item)
{
    cJSON_rsf *child = NULL;

    if ((item == NULL) || (array == NULL))
    {
        return false;
    }

    child = array->child;

    if (child == NULL)
    {
        /* list is empty, start new one */
        array->child = item;
    }
    else
    {
        /* append to the end */
        while (child->next)
        {
            child = child->next;
        }
        suffix_object(child, item);
    }

    return true;
}

/* Add item to array/object. */
void cJSON_rsf_AddItemToArray(cJSON_rsf *array, cJSON_rsf *item)
{
    add_item_to_array(array, item);
}

/* helper function to cast away const */
static void* cast_away_const(const void* string)
{
    return (void*)string;
}

static bool add_item_to_object(cJSON_rsf * const object, const char * const string, cJSON_rsf * const item, const bool constant_key)
{
    char *new_key = NULL;
    int new_type = cJSON_rsf_Invalid;

    if ((object == NULL) || (string == NULL) || (item == NULL))
    {
        return false;
    }

    if (constant_key)
    {
        new_key = (char*) cast_away_const(string);
        new_type = item->type | cJSON_rsf_StringIsConst;
    }
    else
    {
        new_key = (char*) cJSON_rsf_strdup((const unsigned char*)string);
        if (new_key == NULL)
        {
            return false;
        }

        new_type = item->type & ~cJSON_rsf_StringIsConst;
    }

    if (!(item->type & cJSON_rsf_StringIsConst) && (item->string != NULL))
    {
        free(item->string);
    }

    item->string = new_key;
    item->type = new_type;

    return add_item_to_array(object, item);
}

void cJSON_rsf_AddItemToObject(cJSON_rsf *object, const char *string, cJSON_rsf *item)
{
    add_item_to_object(object, string, item, false);
}

/* Add an item to an object with constant string as key */
void cJSON_rsf_AddItemToObjectCS(cJSON_rsf *object, const char *string, cJSON_rsf *item)
{
    add_item_to_object(object, string, item, true);
}

void cJSON_rsf_AddItemReferenceToArray(cJSON_rsf *array, cJSON_rsf *item)
{
    if (array == NULL)
    {
        return;
    }

    add_item_to_array(array, create_reference(item));
}

void cJSON_rsf_AddItemReferenceToObject(cJSON_rsf *object, const char *string, cJSON_rsf *item)
{
    if ((object == NULL) || (string == NULL))
    {
        return;
    }

    add_item_to_object(object, string, create_reference(item), false);
}

cJSON_rsf* cJSON_rsf_AddNullToObject(cJSON_rsf * const object, const char * const name)
{
    cJSON_rsf *null = cJSON_rsf_CreateNull();
    if (add_item_to_object(object, name, null, false))
    {
        return null;
    }

    cJSON_rsf_Delete(null);
    return NULL;
}

cJSON_rsf* cJSON_rsf_AddTrueToObject(cJSON_rsf * const object, const char * const name)
{
    cJSON_rsf *true_item = cJSON_rsf_CreateTrue();
    if (add_item_to_object(object, name, true_item, false))
    {
        return true_item;
    }

    cJSON_rsf_Delete(true_item);
    return NULL;
}

cJSON_rsf* cJSON_rsf_AddFalseToObject(cJSON_rsf * const object, const char * const name)
{
    cJSON_rsf *false_item = cJSON_rsf_CreateFalse();
    if (add_item_to_object(object, name, false_item, false))
    {
        return false_item;
    }

    cJSON_rsf_Delete(false_item);
    return NULL;
}

cJSON_rsf* cJSON_rsf_AddBoolToObject(cJSON_rsf * const object, const char * const name, const bool boolean)
{
    cJSON_rsf *bool_item = cJSON_rsf_CreateBool(boolean);
    if (add_item_to_object(object, name, bool_item, false))
    {
        return bool_item;
    }

    cJSON_rsf_Delete(bool_item);
    return NULL;
}

cJSON_rsf* cJSON_rsf_AddNumberToObject(cJSON_rsf * const object, const char * const name, const float number)
{
    cJSON_rsf *number_item = cJSON_rsf_CreateNumber(number);
    if (add_item_to_object(object, name, number_item, false))
    {
        return number_item;
    }

    cJSON_rsf_Delete(number_item);
    return NULL;
}

cJSON_rsf* cJSON_rsf_AddStringToObject(cJSON_rsf * const object, const char * const name, const char * const string)
{
    cJSON_rsf *string_item = cJSON_rsf_CreateString(string);
    if (add_item_to_object(object, name, string_item, false))
    {
        return string_item;
    }

    cJSON_rsf_Delete(string_item);
    return NULL;
}

cJSON_rsf* cJSON_rsf_AddRawToObject(cJSON_rsf * const object, const char * const name, const char * const raw)
{
    cJSON_rsf *raw_item = cJSON_rsf_CreateRaw(raw);
    if (add_item_to_object(object, name, raw_item, false))
    {
        return raw_item;
    }

    cJSON_rsf_Delete(raw_item);
    return NULL;
}

cJSON_rsf* cJSON_rsf_AddObjectToObject(cJSON_rsf * const object, const char * const name)
{
    cJSON_rsf *object_item = cJSON_rsf_CreateObject();
    if (add_item_to_object(object, name, object_item, false))
    {
        return object_item;
    }

    cJSON_rsf_Delete(object_item);
    return NULL;
}

cJSON_rsf* cJSON_rsf_AddArrayToObject(cJSON_rsf * const object, const char * const name)
{
    cJSON_rsf *array = cJSON_rsf_CreateArray();
    if (add_item_to_object(object, name, array, false))
    {
        return array;
    }

    cJSON_rsf_Delete(array);
    return NULL;
}

cJSON_rsf* cJSON_rsf_DetachItemViaPointer(cJSON_rsf *parent, cJSON_rsf * const item)
{
    if ((parent == NULL) || (item == NULL))
    {
        return NULL;
    }

    if (item->prev != NULL)
    {
        /* not the first element */
        item->prev->next = item->next;
    }
    if (item->next != NULL)
    {
        /* not the last element */
        item->next->prev = item->prev;
    }

    if (item == parent->child)
    {
        /* first element */
        parent->child = item->next;
    }
    /* make sure the detached item doesn't point anywhere anymore */
    item->prev = NULL;
    item->next = NULL;

    return item;
}

cJSON_rsf* cJSON_rsf_DetachItemFromArray(cJSON_rsf *array, int which)
{
    if (which < 0)
    {
        return NULL;
    }

    return cJSON_rsf_DetachItemViaPointer(array, get_array_item(array, (size_t)which));
}

void cJSON_rsf_DeleteItemFromArray(cJSON_rsf *array, int which)
{
    cJSON_rsf_Delete(cJSON_rsf_DetachItemFromArray(array, which));
}

cJSON_rsf* cJSON_rsf_DetachItemFromObject(cJSON_rsf *object, const char *string)
{
    cJSON_rsf *to_detach = cJSON_rsf_GetObjectItem(object, string);

    return cJSON_rsf_DetachItemViaPointer(object, to_detach);
}

cJSON_rsf* cJSON_rsf_DetachItemFromObjectCaseSensitive(cJSON_rsf *object, const char *string)
{
    cJSON_rsf *to_detach = cJSON_rsf_GetObjectItemCaseSensitive(object, string);

    return cJSON_rsf_DetachItemViaPointer(object, to_detach);
}

void cJSON_rsf_DeleteItemFromObject(cJSON_rsf *object, const char *string)
{
    cJSON_rsf_Delete(cJSON_rsf_DetachItemFromObject(object, string));
}

void cJSON_rsf_DeleteItemFromObjectCaseSensitive(cJSON_rsf *object, const char *string)
{
    cJSON_rsf_Delete(cJSON_rsf_DetachItemFromObjectCaseSensitive(object, string));
}

/* Replace array/object items with new ones. */
void cJSON_rsf_InsertItemInArray(cJSON_rsf *array, int which, cJSON_rsf *newitem)
{
    cJSON_rsf *after_inserted = NULL;

    if (which < 0)
    {
        return;
    }

    after_inserted = get_array_item(array, (size_t)which);
    if (after_inserted == NULL)
    {
        add_item_to_array(array, newitem);
        return;
    }

    newitem->next = after_inserted;
    newitem->prev = after_inserted->prev;
    after_inserted->prev = newitem;
    if (after_inserted == array->child)
    {
        array->child = newitem;
    }
    else
    {
        newitem->prev->next = newitem;
    }
}

bool cJSON_rsf_ReplaceItemViaPointer(cJSON_rsf * const parent, cJSON_rsf * const item, cJSON_rsf * replacement)
{
    if ((parent == NULL) || (replacement == NULL) || (item == NULL))
    {
        return false;
    }

    if (replacement == item)
    {
        return true;
    }

    replacement->next = item->next;
    replacement->prev = item->prev;

    if (replacement->next != NULL)
    {
        replacement->next->prev = replacement;
    }
    if (replacement->prev != NULL)
    {
        replacement->prev->next = replacement;
    }
    if (parent->child == item)
    {
        parent->child = replacement;
    }

    item->next = NULL;
    item->prev = NULL;
    cJSON_rsf_Delete(item);

    return true;
}

void cJSON_rsf_ReplaceItemInArray(cJSON_rsf *array, int which, cJSON_rsf *newitem)
{
    if (which < 0)
    {
        return;
    }

    cJSON_rsf_ReplaceItemViaPointer(array, get_array_item(array, (size_t)which), newitem);
}

static bool replace_item_in_object(cJSON_rsf *object, const char *string, cJSON_rsf *replacement, bool case_sensitive)
{
    if ((replacement == NULL) || (string == NULL))
    {
        return false;
    }

    /* replace the name in the replacement */
    if (!(replacement->type & cJSON_rsf_StringIsConst) && (replacement->string != NULL))
    {
        free(replacement->string);
    }
    replacement->string = (char*) cJSON_rsf_strdup((const unsigned char*) string);
    replacement->type &= ~cJSON_rsf_StringIsConst;

    cJSON_rsf_ReplaceItemViaPointer(object, get_object_item(object, string, case_sensitive), replacement);

    return true;
}

void cJSON_rsf_ReplaceItemInObject(cJSON_rsf *object, const char *string, cJSON_rsf *newitem)
{
    replace_item_in_object(object, string, newitem, false);
}

void cJSON_rsf_ReplaceItemInObjectCaseSensitive(cJSON_rsf *object, const char *string, cJSON_rsf *newitem)
{
    replace_item_in_object(object, string, newitem, true);
}

/* Create basic types: */
cJSON_rsf* cJSON_rsf_CreateNull(void)
{
    cJSON_rsf *item = cJSON_rsf_New_Item();
    if(item)
    {
        item->type = cJSON_rsf_NULL;
    }

    return item;
}

cJSON_rsf* cJSON_rsf_CreateTrue(void)
{
    cJSON_rsf *item = cJSON_rsf_New_Item();
    if(item)
    {
        item->type = cJSON_rsf_True;
    }

    return item;
}

cJSON_rsf* cJSON_rsf_CreateFalse(void)
{
    cJSON_rsf *item = cJSON_rsf_New_Item();
    if(item)
    {
        item->type = cJSON_rsf_False;
    }

    return item;
}

cJSON_rsf* cJSON_rsf_CreateBool(bool b)
{
    cJSON_rsf *item = cJSON_rsf_New_Item();
    if(item)
    {
        item->type = b ? cJSON_rsf_True : cJSON_rsf_False;
    }

    return item;
}

cJSON_rsf* cJSON_rsf_CreateNumber(float num)
{
    cJSON_rsf *item = cJSON_rsf_New_Item();
    if (item)
    {
        item->type = cJSON_rsf_Number;
        item->valuefloat = num;
    }

    return item;
}

cJSON_rsf* cJSON_rsf_CreateString(const char *string)
{
    cJSON_rsf *item = cJSON_rsf_New_Item();
    if (item)
    {
        item->type = cJSON_rsf_String;
        item->valuestring = (char*) cJSON_rsf_strdup((const unsigned char*) string);
        if (!item->valuestring)
        {
            cJSON_rsf_Delete(item);
            return NULL;
        }
    }

    return item;
}

cJSON_rsf* cJSON_rsf_CreateStringReference(const char *string)
{
    cJSON_rsf *item = cJSON_rsf_New_Item();
    if (item != NULL)
    {
        item->type = cJSON_rsf_String | cJSON_rsf_IsReference;
        item->valuestring = (char*)cast_away_const(string);
    }

    return item;
}

cJSON_rsf* cJSON_rsf_CreateObjectReference(const cJSON_rsf *child)
{
    cJSON_rsf *item = cJSON_rsf_New_Item();
    if (item != NULL) {
        item->type = cJSON_rsf_Object | cJSON_rsf_IsReference;
        item->child = (cJSON_rsf*)cast_away_const(child);
    }

    return item;
}

cJSON_rsf* cJSON_rsf_CreateArrayReference(const cJSON_rsf *child) {
    cJSON_rsf *item = cJSON_rsf_New_Item();
    if (item != NULL) {
        item->type = cJSON_rsf_Array | cJSON_rsf_IsReference;
        item->child = (cJSON_rsf*)cast_away_const(child);
    }

    return item;
}

cJSON_rsf* cJSON_rsf_CreateRaw(const char *raw)
{
    cJSON_rsf *item = cJSON_rsf_New_Item();
    if(item)
    {
        item->type = cJSON_rsf_Raw;
        item->valuestring = (char*) cJSON_rsf_strdup((const unsigned char*) raw);
        if(!item->valuestring)
        {
            cJSON_rsf_Delete(item);
            return NULL;
        }
    }

    return item;
}

cJSON_rsf* cJSON_rsf_CreateArray(void)
{
    cJSON_rsf *item = cJSON_rsf_New_Item();
    if(item)
    {
        item->type=cJSON_rsf_Array;
    }

    return item;
}

cJSON_rsf* cJSON_rsf_CreateObject(void)
{
    cJSON_rsf *item = cJSON_rsf_New_Item();
    if (item)
    {
        item->type = cJSON_rsf_Object;
    }

    return item;
}

/* Create Arrays: */
cJSON_rsf* cJSON_rsf_CreateIntArray(const int *numbers, int count)
{
    size_t i = 0;
    cJSON_rsf *n = NULL;
    cJSON_rsf *p = NULL;
    cJSON_rsf *a = NULL;

    if ((count < 0) || (numbers == NULL))
    {
        return NULL;
    }

    a = cJSON_rsf_CreateArray();
    for(i = 0; a && (i < (size_t)count); i++)
    {
        n = cJSON_rsf_CreateNumber((float) numbers[i]);
        if (!n)
        {
            cJSON_rsf_Delete(a);
            return NULL;
        }
        if(!i)
        {
            a->child = n;
        }
        else
        {
            suffix_object(p, n);
        }
        p = n;
    }

    return a;
}

cJSON_rsf* cJSON_rsf_CreateFloatArray(const float *numbers, int count)
{
    size_t i = 0;
    cJSON_rsf *n = NULL;
    cJSON_rsf *p = NULL;
    cJSON_rsf *a = NULL;

    if ((count < 0) || (numbers == NULL))
    {
        return NULL;
    }

    a = cJSON_rsf_CreateArray();

    for(i = 0; a && (i < (size_t)count); i++)
    {
        n = cJSON_rsf_CreateNumber(numbers[i]);
        if(!n)
        {
            cJSON_rsf_Delete(a);
            return NULL;
        }
        if(!i)
        {
            a->child = n;
        }
        else
        {
            suffix_object(p, n);
        }
        p = n;
    }

    return a;
}

cJSON_rsf* cJSON_rsf_CreateDoubleArray(const double *numbers, int count)
{
    size_t i = 0;
    cJSON_rsf *n = NULL;
    cJSON_rsf *p = NULL;
    cJSON_rsf *a = NULL;

    if ((count < 0) || (numbers == NULL))
    {
        return NULL;
    }

    a = cJSON_rsf_CreateArray();

    for(i = 0;a && (i < (size_t)count); i++)
    {
        n = cJSON_rsf_CreateNumber((float) numbers[i]);
        if(!n)
        {
            cJSON_rsf_Delete(a);
            return NULL;
        }
        if(!i)
        {
            a->child = n;
        }
        else
        {
            suffix_object(p, n);
        }
        p = n;
    }

    return a;
}

cJSON_rsf* cJSON_rsf_CreateStringArray(const char **strings, int count)
{
    size_t i = 0;
    cJSON_rsf *n = NULL;
    cJSON_rsf *p = NULL;
    cJSON_rsf *a = NULL;

    if ((count < 0) || (strings == NULL))
    {
        return NULL;
    }

    a = cJSON_rsf_CreateArray();

    for (i = 0; a && (i < (size_t)count); i++)
    {
        n = cJSON_rsf_CreateString(strings[i]);
        if(!n)
        {
            cJSON_rsf_Delete(a);
            return NULL;
        }
        if(!i)
        {
            a->child = n;
        }
        else
        {
            suffix_object(p,n);
        }
        p = n;
    }

    return a;
}

/* Duplication */
cJSON_rsf* cJSON_rsf_Duplicate(const cJSON_rsf *item, bool recurse)
{
    cJSON_rsf *newitem = NULL;
    cJSON_rsf *child = NULL;
    cJSON_rsf *next = NULL;
    cJSON_rsf *newchild = NULL;

    /* Bail on bad ptr */
    if (!item)
    {
        goto fail;
    }
    /* Create new item */
    newitem = cJSON_rsf_New_Item();
    if (!newitem)
    {
        goto fail;
    }
    /* Copy over all vars */
    newitem->type = item->type & (~cJSON_rsf_IsReference);
    if (cJSON_rsf_IsString(item) || cJSON_rsf_IsRaw(item))
    {
        if (item->valuestring)
        {
            newitem->valuestring = (char*) cJSON_rsf_strdup((unsigned char*) item->valuestring);
            if (!newitem->valuestring)
            {
                goto fail;
            }
        }
    } else {
        newitem->valuefloat = item->valuefloat;
    }
    if (item->string)
    {
        newitem->string = (item->type & cJSON_rsf_StringIsConst) ? item->string : (char*) cJSON_rsf_strdup((unsigned char*) item->string);
        if (!newitem->string)
        {
            goto fail;
        }
    }
    /* If non-recursive, then we're done! */
    if (!recurse)
    {
        return newitem;
    }
    /* Walk the ->next chain for the child. */
    child = item->child;
    while (child != NULL)
    {
        newchild = cJSON_rsf_Duplicate(child, true); /* Duplicate (with recurse) each item in the ->next chain */
        if (!newchild)
        {
            goto fail;
        }
        if (next != NULL)
        {
            /* If newitem->child already set, then crosswire ->prev and ->next and move on */
            next->next = newchild;
            newchild->prev = next;
            next = newchild;
        }
        else
        {
            /* Set newitem->child and move to it */
            newitem->child = newchild;
            next = newchild;
        }
        child = child->next;
    }

    return newitem;

fail:
    if (newitem != NULL)
    {
        cJSON_rsf_Delete(newitem);
    }

    return NULL;
}

void cJSON_rsf_Minify(char *json)
{
    unsigned char *into = (unsigned char*)json;

    if (json == NULL)
    {
        return;
    }

    while (*json)
    {
        if (*json == ' ')
        {
            json++;
        }
        else if (*json == '\t')
        {
            /* Whitespace characters. */
            json++;
        }
        else if (*json == '\r')
        {
            json++;
        }
        else if (*json=='\n')
        {
            json++;
        }
        else if ((*json == '/') && (json[1] == '/'))
        {
            /* double-slash comments, to end of line. */
            while (*json && (*json != '\n'))
            {
                json++;
            }
        }
        else if ((*json == '/') && (json[1] == '*'))
        {
            /* multiline comments. */
            while (*json && !((*json == '*') && (json[1] == '/')))
            {
                json++;
            }
            json += 2;
        }
        else if (*json == '\"')
        {
            /* string literals, which are \" sensitive. */
            *into++ = (unsigned char)*json++;
            while (*json && (*json != '\"'))
            {
                if (*json == '\\')
                {
                    *into++ = (unsigned char)*json++;
                }
                *into++ = (unsigned char)*json++;
            }
            *into++ = (unsigned char)*json++;
        }
        else
        {
            /* All other characters. */
            *into++ = (unsigned char)*json++;
        }
    }

    /* and null-terminate. */
    *into = '\0';
}

bool cJSON_rsf_IsInvalid(const cJSON_rsf * const item)
{
    if (item == NULL)
    {
        return false;
    }

    return (item->type & 0xFF) == cJSON_rsf_Invalid;
}

bool cJSON_rsf_IsFalse(const cJSON_rsf * const item)
{
    if (item == NULL)
    {
        return false;
    }

    return (item->type & 0xFF) == cJSON_rsf_False;
}

bool cJSON_rsf_IsTrue(const cJSON_rsf * const item)
{
    if (item == NULL)
    {
        return false;
    }

    return (item->type & 0xff) == cJSON_rsf_True;
}


bool cJSON_rsf_IsBool(const cJSON_rsf * const item)
{
    if (item == NULL)
    {
        return false;
    }

    return (item->type & (cJSON_rsf_True | cJSON_rsf_False)) != 0;
}
bool cJSON_rsf_IsNull(const cJSON_rsf * const item)
{
    if (item == NULL)
    {
        return false;
    }

    return (item->type & 0xFF) == cJSON_rsf_NULL;
}

bool cJSON_rsf_IsNumber(const cJSON_rsf * const item)
{
    if (item == NULL)
    {
        return false;
    }

    return (item->type & 0xFF) == cJSON_rsf_Number;
}

bool cJSON_rsf_IsString(const cJSON_rsf * const item)
{
    if (item == NULL)
    {
        return false;
    }

    return (item->type & 0xFF) == cJSON_rsf_String;
}

bool cJSON_rsf_IsArray(const cJSON_rsf * const item)
{
    if (item == NULL)
    {
        return false;
    }

    return (item->type & 0xFF) == cJSON_rsf_Array;
}

bool cJSON_rsf_IsObject(const cJSON_rsf * const item)
{
    if (item == NULL)
    {
        return false;
    }

    return (item->type & 0xFF) == cJSON_rsf_Object;
}

bool cJSON_rsf_IsRaw(const cJSON_rsf * const item)
{
    if (item == NULL)
    {
        return false;
    }

    return (item->type & 0xFF) == cJSON_rsf_Raw;
}

bool cJSON_rsf_Compare(const cJSON_rsf * const a, const cJSON_rsf * const b, const bool case_sensitive)
{
    if ((a == NULL) || (b == NULL) || ((a->type & 0xFF) != (b->type & 0xFF)) || cJSON_rsf_IsInvalid(a))
    {
        return false;
    }

    /* check if type is valid */
    switch (a->type & 0xFF)
    {
        case cJSON_rsf_False:
        case cJSON_rsf_True:
        case cJSON_rsf_NULL:
        case cJSON_rsf_Number:
        case cJSON_rsf_String:
        case cJSON_rsf_Raw:
        case cJSON_rsf_Array:
        case cJSON_rsf_Object:
            break;

        default:
            return false;
    }

    /* identical objects are equal */
    if (a == b)
    {
        return true;
    }

    switch (a->type & 0xFF)
    {
        /* in these cases and equal type is enough */
        case cJSON_rsf_False:
        case cJSON_rsf_True:
        case cJSON_rsf_NULL:
            return true;

        case cJSON_rsf_Number:
            if (a->valuefloat == b->valuefloat)
            {
                return true;
            }
            return false;

        case cJSON_rsf_String:
        case cJSON_rsf_Raw:
            if ((a->valuestring == NULL) || (b->valuestring == NULL))
            {
                return false;
            }
            if (strcmp(a->valuestring, b->valuestring) == 0)
            {
                return true;
            }

            return false;

        case cJSON_rsf_Array:
        {
            cJSON_rsf *a_element = a->child;
            cJSON_rsf *b_element = b->child;

            for (; (a_element != NULL) && (b_element != NULL);)
            {
                if (!cJSON_rsf_Compare(a_element, b_element, case_sensitive))
                {
                    return false;
                }

                a_element = a_element->next;
                b_element = b_element->next;
            }

            /* one of the arrays is longer than the other */
            if (a_element != b_element) {
                return false;
            }

            return true;
        }

        case cJSON_rsf_Object:
        {
            cJSON_rsf *a_element = NULL;
            cJSON_rsf *b_element = NULL;
            cJSON_rsf_ArrayForEach(a_element, a)
            {
                /* TODO This has O(n^2) runtime, which is horrible! */
                b_element = get_object_item(b, a_element->string, case_sensitive);
                if (b_element == NULL)
                {
                    return false;
                }

                if (!cJSON_rsf_Compare(a_element, b_element, case_sensitive))
                {
                    return false;
                }
            }

            /* doing this twice, once on a and b to prevent true comparison if a subset of b
             * TODO: Do this the proper way, this is just a fix for now */
            cJSON_rsf_ArrayForEach(b_element, b)
            {
                a_element = get_object_item(a, b_element->string, case_sensitive);
                if (a_element == NULL)
                {
                    return false;
                }

                if (!cJSON_rsf_Compare(b_element, a_element, case_sensitive))
                {
                    return false;
                }
            }

            return true;
        }

        default:
            return false;
    }
}

void* cJSON_rsf_malloc(size_t size)
{
    return malloc(size);
}

void cJSON_rsf_free(void *object)
{
    free(object);
}
