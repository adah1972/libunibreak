/*
 * Grapheme breaking in a Unicode sequence.  Designed to be used in a
 * generic text renderer.
 *
 * Copyright (C) 2018 Andreas Röver <roever at users dot sf dot net>
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
 *      <URL:http://www.unicode.org/reports/tr29/tr29-29.html>
 *
 * This library has been updated according to Revision 33, for
 * Unicode 11.0.0:
 *      <URL:http://www.unicode.org/reports/tr29/tr29-33.html>
 *
 * The Unicode Terms of Use are available at
 *      <URL:http://www.unicode.org/copyright.html>
 */

/**
 * @file    emojidef.c
 *
 * Emoji-related routines.
 *
 * @author  Andreas Röver
 */

#include "emojidef.h"
#include "emojidata.c"

#define ARRAY_LEN(x) (sizeof(x) / sizeof(x[0]))

/**
 * Finds out if a codepoint is an extended pictographic codepoint.
 *
 * @param[in] ch  character to check
 * @return        \c true if the codepoint is extended pictographic;
 *                \c false otherwise
 */
bool is_char_extended_pictographic(utf32_t ch)
{
    int min = 0;
    int max = ARRAY_LEN(ep_prop) - 1;
    int mid;

    do
    {
        mid = (min + max) / 2;

        if (ch < ep_prop[mid].start)
            max = mid - 1;
        else if (ch > ep_prop[mid].end)
            min = mid + 1;
        else
            return true;
    } while (min <= max);

    return false;
}
