/**
 * LCD/OLED fonts library
 *
 * FIXME: License?
 *
 * @date: 8 dec. 2016
 *      Author: zaltora
 */
#include "fonts.h"

#ifndef FONTS_GLCD_5X7
#define FONTS_GLCD_5X7 1
#endif

#if FONTS_GLCD_5X7
    #include "data/font_glcd_5x7.h"
#endif

#if FONTS_ROBOTO_8PT
    #include "data/font_roboto_8pt.h"
#endif
#if FONTS_ROBOTO_10PT
    #include "data/font_roboto_10pt.h"
#endif

#if FONTS_BITOCRA_4X7
    #include "data/font_bitocra_4x7_ascii.h"
#endif
#if FONTS_BITOCRA_6X11
    #include "data/font_bitocra_6x11_iso8859_1.h"
#endif
#if FONTS_BITOCRA_7X13
    #include "data/font_bitocra_7x13_iso8859_1.h"
#endif

#if FONTS_TERMINUS_6X12_ISO8859_1
    #include "data/font_terminus_6x12_iso8859_1.h"
#endif
#if FONTS_TERMINUS_8X14_ISO8859_1
    #include "data/font_terminus_8x14_iso8859_1.h"
#endif
#if FONTS_TERMINUS_BOLD_8X14_ISO8859_1
    #include "data/font_terminus_bold_8x14_iso8859_1.h"
#endif
#if FONTS_TERMINUS_10X18_ISO8859_1
    #include "data/font_terminus_10x18_iso8859_1.h"
#endif
#if FONTS_TERMINUS_BOLD_10X18_ISO8859_1
    #include "data/font_terminus_bold_10x18_iso8859_1.h"
#endif
#if FONTS_TERMINUS_11X22_ISO8859_1
    #include "data/font_terminus_11x22_iso8859_1.h"
#endif
#if FONTS_TERMINUS_BOLD_11X22_ISO8859_1
    #include "data/font_terminus_bold_11x22_iso8859_1.h"
#endif
#if FONTS_TERMINUS_12X24_ISO8859_1
    #include "data/font_terminus_12x24_iso8859_1.h"
#endif
#if FONTS_TERMINUS_BOLD_12X24_ISO8859_1
    #include "data/font_terminus_bold_12x24_iso8859_1.h"
#endif
#if FONTS_TERMINUS_14X28_ISO8859_1
    #include "data/font_terminus_14x28_iso8859_1.h"
#endif
#if FONTS_TERMINUS_BOLD_14X28_ISO8859_1
    #include "data/font_terminus_bold_14x28_iso8859_1.h"
#endif
#if FONTS_TERMINUS_16X32_ISO8859_1
    #include "data/font_terminus_16x32_iso8859_1.h"
#endif
#if FONTS_TERMINUS_BOLD_16X32_ISO8859_1
    #include "data/font_terminus_bold_16x32_iso8859_1.h"
#endif

#if FONTS_TERMINUS_6X12_KOI8_R
    #include "data/font_terminus_6x12_koi8_r.h"
#endif
#if FONTS_TERMINUS_8X14_KOI8_R
    #include "data/font_terminus_8x14_koi8_r.h"
#endif
#if FONTS_TERMINUS_BOLD_8X14_KOI8_R
    #include "data/font_terminus_bold_8x14_koi8_r.h"
#endif
#if FONTS_TERMINUS_14X28_KOI8_R
    #include "data/font_terminus_14x28_koi8_r.h"
#endif
#if FONTS_TERMINUS_BOLD_14X28_KOI8_R
    #include "data/font_terminus_bold_14x28_koi8_r.h"
#endif
#if FONTS_TERMINUS_16X32_KOI8_R
    #include "data/font_terminus_16x32_koi8_r.h"
#endif
#if FONTS_TERMINUS_BOLD_16X32_KOI8_R
    #include "data/font_terminus_bold_16x32_koi8_r.h"
#endif

/////////////////////////////////////////////

// FIXME: this declaration is noisy

const font_info_t *font_builtin_fonts[] =
{
#if FONTS_GLCD_5X7
    [FONT_FACE_GLCD5x7] = &_fonts_glcd_5x7_info,
#else
    [FONT_FACE_GLCD5x7] = NULL,
#endif

#if FONTS_ROBOTO_8PT
    [FONT_FACE_ROBOTO_8PT] = &_fonts_roboto_8pt_info,
#else
    [FONT_FACE_ROBOTO_8PT] = NULL,
#endif
#if FONTS_ROBOTO_10PT
    [FONT_FACE_ROBOTO_10PT] = &_fonts_roboto_10pt_info,
#else
    [FONT_FACE_ROBOTO_10PT] = NULL,
#endif

#if FONTS_BITOCRA_4X7
    [FONT_FACE_BITOCRA_4X7] = &_fonts_bitocra_4x7_ascii_info,
#else
    [FONT_FACE_BITOCRA_4X7] = NULL,
#endif
#if FONTS_BITOCRA_6X11
    [FONT_FACE_BITOCRA_6X11] = &_fonts_bitocra_6x11_iso8859_1_info,
#else
    [FONT_FACE_BITOCRA_6X11] = NULL,
#endif
#if FONTS_BITOCRA_7X13
    [FONT_FACE_BITOCRA_7X13] = &_fonts_bitocra_7x13_iso8859_1_info,
#else
    [FONT_FACE_BITOCRA_7X13] = NULL,
#endif

#if FONTS_TERMINUS_6X12_ISO8859_1
    [FONT_FACE_TERMINUS_6X12_ISO8859_1] = &_fonts_terminus_6x12_iso8859_1_info,
#else
    [FONT_FACE_TERMINUS_6X12_ISO8859_1] = NULL,
#endif
#if FONTS_TERMINUS_8X14_ISO8859_1
    [FONT_FACE_TERMINUS_8X14_ISO8859_1] = &_fonts_terminus_8x14_iso8859_1_info,
#else
    [FONT_FACE_TERMINUS_8X14_ISO8859_1] = NULL,
#endif
#if FONTS_TERMINUS_BOLD_8X14_ISO8859_1
    [FONT_FACE_TERMINUS_BOLD_8X14_ISO8859_1] = &_fonts_terminus_bold_8x14_iso8859_1_info,
#else
    [FONT_FACE_TERMINUS_BOLD_8X14_ISO8859_1] = NULL,
#endif
#if FONTS_TERMINUS_10X18_ISO8859_1
    [FONT_FACE_TERMINUS_10X18_ISO8859_1] = &_fonts_terminus_10x18_iso8859_1_info,
#else
    [FONT_FACE_TERMINUS_10X18_ISO8859_1] = NULL,
#endif
#if FONTS_TERMINUS_BOLD_10X18_ISO8859_1
    [FONT_FACE_TERMINUS_BOLD_10X18_ISO8859_1] = &_fonts_terminus_bold_10x18_iso8859_1_info,
#else
    [FONT_FACE_TERMINUS_BOLD_10X18_ISO8859_1] = NULL,
#endif
#if FONTS_TERMINUS_11X22_ISO8859_1
    [FONT_FACE_TERMINUS_11X22_ISO8859_1] = &_fonts_terminus_11x22_iso8859_1_info,
#else
    [FONT_FACE_TERMINUS_11X22_ISO8859_1] = NULL,
#endif
#if FONTS_TERMINUS_BOLD_11X22_ISO8859_1
    [FONT_FACE_TERMINUS_BOLD_11X22_ISO8859_1] = &_fonts_terminus_bold_11x22_iso8859_1_info,
#else
    [FONT_FACE_TERMINUS_BOLD_11X22_ISO8859_1] = NULL,
#endif
#if FONTS_TERMINUS_12X24_ISO8859_1
    [FONT_FACE_TERMINUS_12X24_ISO8859_1] = &_fonts_terminus_12x24_iso8859_1_info,
#else
    [FONT_FACE_TERMINUS_12X24_ISO8859_1] = NULL,
#endif
#if FONTS_TERMINUS_BOLD_12X24_ISO8859_1
    [FONT_FACE_TERMINUS_BOLD_12X24_ISO8859_1] = &_fonts_terminus_bold_12x24_iso8859_1_info,
#else
    [FONT_FACE_TERMINUS_BOLD_12X24_ISO8859_1] = NULL,
#endif
#if FONTS_TERMINUS_14X28_ISO8859_1
    [FONT_FACE_TERMINUS_14X28_ISO8859_1] = &_fonts_terminus_14x28_iso8859_1_info,
#else
    [FONT_FACE_TERMINUS_14X28_ISO8859_1] = NULL,
#endif
#if FONTS_TERMINUS_BOLD_14X28_ISO8859_1
    [FONT_FACE_TERMINUS_BOLD_14X28_ISO8859_1] = &_fonts_terminus_bold_14x28_iso8859_1_info,
#else
    [FONT_FACE_TERMINUS_BOLD_14X28_ISO8859_1] = NULL,
#endif
#if FONTS_TERMINUS_16X32_ISO8859_1
    [FONT_FACE_TERMINUS_16X32_ISO8859_1] = &_fonts_terminus_16x32_iso8859_1_info,
#else
    [FONT_FACE_TERMINUS_16X32_ISO8859_1] = NULL,
#endif
#if FONTS_TERMINUS_BOLD_16X32_ISO8859_1
    [FONT_FACE_TERMINUS_BOLD_16X32_ISO8859_1] = &_fonts_terminus_bold_16x32_iso8859_1_info,
#else
    [FONT_FACE_TERMINUS_BOLD_16X32_ISO8859_1] = NULL,
#endif

#if FONTS_TERMINUS_6X12_KOI8_R
    [FONT_FACE_TERMINUS_6X12_KOI8_R] = &_fonts_terminus_6x12_koi8_r_info,
#else
    [FONT_FACE_TERMINUS_6X12_KOI8_R] = NULL,
#endif
#if FONTS_TERMINUS_8X14_KOI8_R
    [FONT_FACE_TERMINUS_8X14_KOI8_R] = &_fonts_terminus_8x14_koi8_r_info,
#else
    [FONT_FACE_TERMINUS_8X14_KOI8_R] = NULL,
#endif
#if FONTS_TERMINUS_BOLD_8X14_KOI8_R
    [FONT_FACE_TERMINUS_BOLD_8X14_KOI8_R] = &_fonts_terminus_bold_8x14_koi8_r_info,
#else
    [FONT_FACE_TERMINUS_BOLD_8X14_KOI8_R] = NULL,
#endif
#if FONTS_TERMINUS_14X28_KOI8_R
    [FONT_FACE_TERMINUS_14X28_KOI8_R] = &_fonts_terminus_14x28_koi8_r_info,
#else
    [FONT_FACE_TERMINUS_14X28_KOI8_R] = NULL,
#endif
#if FONTS_TERMINUS_BOLD_14X28_KOI8_R
    [FONT_FACE_TERMINUS_BOLD_14X28_KOI8_R] = &_fonts_terminus_bold_14x28_koi8_r_info,
#else
    [FONT_FACE_TERMINUS_BOLD_14X28_KOI8_R] = NULL,
#endif
#if FONTS_TERMINUS_16X32_KOI8_R
    [FONT_FACE_TERMINUS_16X32_KOI8_R] = &_fonts_terminus_16x32_koi8_r_info,
#else
    [FONT_FACE_TERMINUS_16X32_KOI8_R] = NULL,
#endif
#if FONTS_TERMINUS_BOLD_16X32_KOI8_R
    [FONT_FACE_TERMINUS_BOLD_16X32_KOI8_R] = &_fonts_terminus_bold_16x32_koi8_r_info,
#else
    [FONT_FACE_TERMINUS_BOLD_16X32_KOI8_R] = NULL,
#endif
};

const size_t font_builtin_fonts_count = (sizeof(font_builtin_fonts) / sizeof(font_info_t *));

/////////////////////////////////////////////

uint16_t font_measure_string(const font_info_t *fnt, const char *s)
{
    if (!s || !fnt) return 0;

    uint16_t res = 0;
    while (*s)
    {
        const font_char_desc_t *d = font_get_char_desc(fnt, *s);
        if (d)
            res += d->width + fnt->c;
        s++;
    }

    return res > 0 ? res - fnt->c : 0;
}

