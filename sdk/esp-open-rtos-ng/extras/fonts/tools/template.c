{%- set header_id -%}
_EXTRAS_FONTS_FONT_{{ font.name|upper }}_{{ font.size[0] }}X{{ font.size[1] }}_{{ font.charset|upper }}_H_
{%- endset -%}
{%- set font_size -%}
{{ font.size[0] }}x{{ font.size[1] }}
{%- endset -%}
{%- set font_prefix -%}
_fonts_{{ font.name|lower }}_{{ font_size }}_{{ font.charset|lower }}
{%- endset -%}
/**
 * This file contains generated binary font data.
 *
 * Font:    {{ font.name }}
 * Size:    {{ font_size }}
 * Charset: {{ font.charset }}
 * {{ chars|length }} characters ({{ font.first }}..{{ font.last }})
 *
 * License: FIXME
 *
 * Generated: {{ created }}
 */
#ifndef {{ header_id }}
#define {{ header_id }}

static const uint8_t {{ font_prefix }}_bitmaps[] = {
    {%- for char in chars %}

    /* {{ 'Index: 0x%02x, char: \\x%02x, offset: 0x%04x'|format(char.index, char.code, char.offset) }} */
    {%- for row in char.rows %}
    {% for byte in row.data %}{{ '0x%02x'|format(byte) }}, {% endfor -%} /* {{ row.asc }} */
    {%- endfor -%}
    {%- endfor %}
};

const font_char_desc_t {{ font_prefix }}_descriptors[] = {
    {%- for char in chars %}
    { {{ '0x%02x'|format(font.size[0]) }}, {{ '0x%04x'|format(char.offset) }} }, /* {{ 'Index: 0x%02x, char: \\x%02x'|format(char.index, char.code) }} */
    {%- endfor %}
};

const font_info_t {{ font_prefix }}_info =
{
    .height           = {{ font.size[1] }}, /* Character height */
    .c                = 0, /* C */
    .char_start       = {{ font.first }}, /* Start character */
    .char_end         = {{ font.last }}, /* End character */
    .char_descriptors = {{ font_prefix }}_descriptors, /* Character descriptor array */
    .bitmap           = {{ font_prefix }}_bitmaps, /* Character bitmap array */
};

#endif /* {{ header_id }} */
