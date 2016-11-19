/*
 * Grapheme breaking in a Unicode sequence.  Designed to be used in a
 * generic text renderer.
 *
 * Copyright (C) 2016 Andreas Röver <roever at users dot sf dot net>
 *
 * This software is provided 'as-is', without any express or implied
 * warranty.  In no event will the author be held liable for any damages
 * arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute
 * it freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you must
 *    not claim that you wrote the original software.  If you use this
 *    software in a product, an acknowledgement in the product
 *    documentation would be appreciated but is not required.
 * 2. Altered source versions must be plainly marked as such, and must
 *    not be misrepresented as being the original software.
 * 3. This notice may not be removed or altered from any source
 *    distribution.
 *
 * The main reference is Unicode Standard Annex 29 (UAX #29):
 *      <URL:http://unicode.org/reports/tr29>
 *
 * When this library was designed, this annex was at Revision 29, for
 * Unicode 9.0.0:
 *
 * The Unicode Terms of Use are available at
 *      <URL:http://www.unicode.org/copyright.html>
 */

/**
 * @file    graphemebreak.c
 *
 * Implementation of the grapheme breaking algorithm as described in Unicode
 * Standard Annex 29.
 *
 * @version 1.0, 2016
 * @author  Andreas Roever
 */

#include "unibreakdef.h"
#include "graphemebreak.h"
#include "graphemebreakdata.c"
#include "stdbool.h"

#define ARRAY_LEN(x) (sizeof(x) / sizeof(x[0]))

/**
 * Initializes the wordbreak internals.  It currently does nothing, but
 * it may in the future.
 */
void init_graphemebreak(void)
{
}

/**
 * Gets the grapheme breaking class of a character.
 *
 * @param ch   character to check
 * @return     the grapheme breaking class if found; \c GBP_Other otherwise
 */
static enum GraphemeBreakClass get_char_gb_class(utf32_t ch)
{
    int min = 0;
    int max = ARRAY_LEN(gb_prop_default) - 1;
    int mid;

    do
    {
        mid = (min + max) / 2;

        if (ch < gb_prop_default[mid].start)
            max = mid - 1;
        else if (ch > gb_prop_default[mid].end)
            min = mid + 1;
        else
            return gb_prop_default[mid].prop;
    }
    while (min <= max);

    return GBP_Other;
}

/**
 * Sets the grapheme breaking information for a generic input string.
 *
 * @param[in]  s             input string
 * @param[in]  len           length of the input
 * @param[out] brks          pointer to the output breaking data, containing
 *                           #GRAPHEMEBREAK_BREAK or #GRAPHEMEBREAK_NOBREAK
 * @param[in] get_next_char  function to get the next UTF-32 character
 */
static void set_graphemebreaks(
        const void *s,
        size_t len,
        char *brks,
        get_next_char_t get_next_char)
{
    size_t posNext = 0;
    bool rule10Left = false; // is the left side of rule 10 fulfilled?
    bool EvenRegionalIndicators = true; // is the number of preceeding GBP_RegionalIndicator characters even

    utf32_t ch = get_next_char(s, len, &posNext);
    enum GraphemeBreakClass a = get_char_gb_class(ch);

    ch = get_next_char(s, len, &posNext);
    while (ch != EOS)
    {
        enum GraphemeBreakClass b = get_char_gb_class(ch);

        if (a == GBP_E_Base || a == GBP_E_Base_GAZ)
          rule10Left = true;
        else if (a != GBP_Extend)
          rule10Left = false;

        if (a == GBP_Regional_Indicator)
          EvenRegionalIndicators = !EvenRegionalIndicators;
        else
          EvenRegionalIndicators = true;

        if (a == GBP_CR && b == GBP_LF)
            *brks = GRAPHEMEBREAK_NOBREAK;            // Rule: GB3
        else if (a == GBP_CR || a == GBP_LF || a == GBP_Control || b == GBP_CR || b == GBP_LF || b == GBP_Control)
            *brks = GRAPHEMEBREAK_BREAK;              // Rule: GB4 + GB5
        else if (a == GBP_L && (b == GBP_L || b == GBP_V || b == GBP_LV || b == GBP_LVT))
            *brks = GRAPHEMEBREAK_NOBREAK;            // Rule: GB6
        else if ((a == GBP_LV || a == GBP_V) && (b == GBP_V || b == GBP_T))
            *brks = GRAPHEMEBREAK_NOBREAK;            // Rule: GB7
        else if ((a == GBP_LVT || a == GBP_T) && (b == GBP_T))
            *brks = GRAPHEMEBREAK_NOBREAK;            // Rule: GB8
        else if (b == GBP_Extend || b == GBP_ZWJ)
            *brks = GRAPHEMEBREAK_NOBREAK;            // Rule: GB9
        else if (b == GBP_SpacingMark)
            *brks = GRAPHEMEBREAK_NOBREAK;            // Rule: GB9a
        else if (a == GBP_Prepend)
            *brks = GRAPHEMEBREAK_NOBREAK;            // Rule: GB9b
        else if (rule10Left && b == GBP_E_Modifier)
            *brks = GRAPHEMEBREAK_NOBREAK;            // Rule: GB10
        else if (a == GBP_ZWJ && (b == GBP_Glue_After_Zwj || b == GBP_E_Base_GAZ))
            *brks = GRAPHEMEBREAK_NOBREAK;            // Rule: GB11
        else if (!EvenRegionalIndicators && (b == GBP_Regional_Indicator))
            *brks = GRAPHEMEBREAK_NOBREAK;            // Rule: GB12 + GB13
        else
            *brks = GRAPHEMEBREAK_BREAK;              // Rule: GB999

        brks++;
        a = b;
        ch = get_next_char(s, len, &posNext);
    }
}

/**
 * Sets the grapheme breaking information for a UTF-8 input string.
 *
 * @param[in]  s     input UTF-8 string
 * @param[in]  len   length of the input
 * @param[in]  lang  language of the input, right now this does not influence the algorithm
 * @param[out] brks  pointer to the output breaking data, containing
 *                   #GRAPHEMEBREAK_BREAK or #GRAPHEMEBREAK_NOBREAK.
 *                   First element in output array is for the break behind the first character
 *                   the pointer must point to an array with at least as many elements as there
 *                   are characters in the string
 */
void set_graphemebreaks_utf8(
        const utf8_t *s,
        size_t len,
        const char *lang,
        char *brks)
{
    set_graphemebreaks(s, len, brks,
                   (get_next_char_t)ub_get_next_char_utf8);
}

/**
 * Sets the grapheme breaking information for a UTF-16 input string.
 *
 * @param[in]  s     input UTF-16 string
 * @param[in]  len   length of the input
 * @param[in]  lang  language of the input, right now this does not influence the algorithm
 * @param[out] brks  pointer to the output breaking data, containing
 *                   #GRAPHEMEBREAK_BREAK or #GRAPHEMEBREAK_NOBREAK.
 *                   First element in output array is for the break behind the first character
 *                   the pointer must point to an array with at least as many elements as there
 *                   are characters in the string
 */
void set_graphemebreaks_utf16(
        const utf16_t *s,
        size_t len,
        const char *lang,
        char *brks)
{
    set_graphemebreaks(s, len, brks,
                   (get_next_char_t)ub_get_next_char_utf16);
}

/**
 * Sets the grapheme breaking information for a UTF-32 input string.
 *
 * @param[in]  s     input UTF-32 string
 * @param[in]  len   length of the input
 * @param[in]  lang  language of the input, right now this does not influence the algorithm
 * @param[out] brks  pointer to the output breaking data, containing
 *                   #GRAPHEMEBREAK_BREAK or #GRAPHEMEBREAK_NOBREAK.
 *                   First element in output array is for the break behind the first character
 *                   the pointer must point to an array with at least as many elements as there
 *                   are characters in the string
 */
void set_graphemebreaks_utf32(
        const utf32_t *s,
        size_t len,
        const char *lang,
        char *brks)
{
    set_graphemebreaks(s, len, brks,
                   (get_next_char_t)ub_get_next_char_utf32);
}
