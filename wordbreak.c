/* vim: set tabstop=4 shiftwidth=4: */

/*
 * Word breaking in a Unicode sequence.  Designed to be used in a
 * generic text renderer.
 *
 * Copyright (C) 2011-2011 Tom Hacohen <tom@stosb.com>
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
 *		<URL:http://unicode.org/reports/tr29>
 *
 * When this library was designed, this annex was at Revision 17, for
 * Unicode 6.0.0:
 *		<URL:http://www.unicode.org/reports/tr29/tr29-17.html>
 *
 * The Unicode Terms of Use are available at
 *		<URL:http://www.unicode.org/copyright.html>
 */

/**
 * @file	wordbreak.c
 *
 * Implementation of the word breaking algorithm as described in Unicode
 * Standard Annex 29.
 *
 * @version	2.0, 2011/12/12
 * @author	Tom Hacohen
 */


#include <assert.h>
#include <stddef.h>
#include <string.h>
#include "linebreak.h"
#include "linebreakdef.h"

#include "wordbreak.h"
#include "wordbreakdata.c"

#define ARRAY_LEN(x) (sizeof(x) / sizeof(x[0]))

/* Init the wordbreak internals. */
void init_wordbreak(void)
{
	/* Currently does nothing, may be needed in the future. */
	return;
}

/**
 * Gets the word breaking class of a character.
 *
 * @param ch	character to check
 * @param wbp	pointer to the wbp breaking properties array
 * @param len	the size of the wbp array in number of items.
 * @return		the word breaking class if found; \c WBP_Any otherwise
 */
static enum WordBreakClass get_char_wb_class(
		utf32_t ch,
		struct WordBreakProperties *wbp,
		size_t len)
{
	int min = 0;
	int max = len - 1;
	int mid;

	do
	{
		mid = (min + max) / 2;

		if (ch < wbp[mid].start)
			max = mid - 1;
		else if (ch > wbp[mid].end)
			min = mid + 1;
		else
			return wbp[mid].prop;
	}
	while (min <= max);

	return WBP_Any;
}

/**
 * Sets the break types in brks starting from posLast up to posStop.
 *
 * It sets the inside chars to #WORDBREAK_INSIDECHAR and the rest to brkType.
 * Assumes brks is initialized - all the cells with #WORDBREAK_NOBREAK are
 * cells that we really don't want to break after.
 *
 * @param s				the string
 * @param brks[out]		the breaks array to fill.
 * @param posStart		the start position
 * @param posEnd		the end position
 * @param len			the length of the string
 * @param brkType		the breaks type to use
 * @param get_next_char	function to get the next UTF-32 character
 */
static void set_brks_to(const void *s,
		char *brks,
		size_t posStart,
		size_t posEnd,
		size_t len,
		char brkType,
		get_next_char_t get_next_char)
{
	size_t posCur = posStart;
	while (posCur < posEnd)
	{
		get_next_char(s, len, &posCur);
		for ( ; posStart < posCur - 1; ++posStart)
		{
			brks[posStart] = WORDBREAK_INSIDECHAR;
		}
		assert(posStart == posCur - 1);

		/* Only set it if we haven't set it not to break before. */
		if (brks[posStart] != WORDBREAK_NOBREAK)
			brks[posStart] = brkType;
		posStart = posCur;
	}
}

/* Checks to see if newline, cr, or lf. for WB3a and b */
#define IS_WB3ab(cls) ((cls == WBP_Newline) || (cls == WBP_CR) || \
		(cls == WBP_LF))

/**
 * Sets the word breaking information for a generic input string.
 *
 * @param[in]  s			input string
 * @param[in]  len			length of the input
 * @param[in]  lang			language of the input
 * @param[out] brks			pointer to the output breaking data, containing
 *							#WORDBREAK_BREAK, #WORDBREAK_NOBREAK, or
 *							#WORDBREAK_INSIDEACHAR
 * @param[in] get_next_char	function to get the next UTF-32 character
 */
static void set_wordbreaks(
		const void *s,
		size_t len,
		const char *lang,
		char *brks,
		get_next_char_t get_next_char)
{
	/* Previous class */
	enum WordBreakClass p_cls = WBP_Undefined;
	/* Strong previous class.
     * Strong class is a class that can start a sequence.
     * Which means, it's all the classes, except for:
     * 1. MidNumLet, MidNum and MidLet.
     * 2. Extend and Format if not at the start of the buffer. */
	enum WordBreakClass sp_cls = WBP_Undefined;
	utf32_t ch;
	size_t posCur = 0;
	size_t posCurSt = 0;
	size_t posLast = 0;

	/* FIXME: unused atm. */
	(void) lang;


	/* Init brks */
	memset(brks, WORDBREAK_BREAK, len);

	ch = get_next_char(s, len, &posCur);

	/* WB3a, WB3b are implied. */
	for ( ; ch != EOS ; )
	{
		/* Current class */
		enum WordBreakClass c_cls;
		c_cls = get_char_wb_class(ch, wb_prop_default,
				ARRAY_LEN(wb_prop_default));

		switch (c_cls)
		{
	    case WBP_CR:
			set_brks_to(s, brks, posLast, posCurSt, len, WORDBREAK_BREAK,
					get_next_char);
			sp_cls = c_cls;
			posLast = posCurSt;
			break;

	    case WBP_LF:
			if (sp_cls == WBP_CR) /* WB3 */
			{
				set_brks_to(s, brks, posLast, posCurSt, len, WORDBREAK_NOBREAK,
						get_next_char);
				sp_cls = c_cls;
				posLast = posCurSt;
			}
			sp_cls = c_cls;
			posLast = posCurSt;
			break;

	    case WBP_Newline:
			/* WB3a, WB3b */
			set_brks_to(s, brks, posLast, posCurSt, len, WORDBREAK_BREAK,
					get_next_char);
			sp_cls = c_cls;
			posLast = posCurSt;
			break;

	    case WBP_Extend:
	    case WBP_Format:
			/* WB4 - If not the first char/after a newline (W3ab),
			 * skip this class, set it to be the same as the prev, and mark
			 * brks not to break before them. */
			if ((sp_cls == WBP_Undefined) || IS_WB3ab(sp_cls))
			{
				set_brks_to(s, brks, posLast, posCurSt, len, WORDBREAK_BREAK,
						get_next_char);
				sp_cls = c_cls;
			}
			else
			{
				/* It's surely not the first */
				brks[posCurSt - 1] = WORDBREAK_NOBREAK;
				/* "inherit" the previous class. */
				c_cls = p_cls;
			}
			break;

	    case WBP_Katakana:
			if ((sp_cls == WBP_Katakana) || /* WB13 */
					(sp_cls == WBP_ExtendNumLet)) /* WB13b */
			{
				set_brks_to(s, brks, posLast, posCurSt, len, WORDBREAK_NOBREAK,
						get_next_char);
			}
			/* No rule found, reset */
			else
			{
				set_brks_to(s, brks, posLast, posCurSt, len, WORDBREAK_BREAK,
						get_next_char);
			}
			sp_cls = c_cls;
			posLast = posCurSt;
			break;

	    case WBP_ALetter:
			if ((sp_cls == WBP_ALetter) || /* WB5,6,7 */
					((sp_cls == WBP_Numeric) && (p_cls == WBP_Numeric)) || /* WB10 */
					(sp_cls == WBP_ExtendNumLet)) /* WB13b */
			{
				set_brks_to(s, brks, posLast, posCurSt, len, WORDBREAK_NOBREAK,
						get_next_char);
			}
			/* No rule found, reset */
			else
			{
				set_brks_to(s, brks, posLast, posCurSt, len, WORDBREAK_BREAK,
						get_next_char);
			}
			sp_cls = c_cls;
			posLast = posCurSt;
			break;

	    case WBP_MidNumLet:
			if ((p_cls == WBP_ALetter) || /* WBP6,7 */
					(p_cls == WBP_Numeric)) /* WBP11,12 */
			{
				/* Go on */
			}
			else
			{
				set_brks_to(s, brks, posLast, posCurSt, len, WORDBREAK_BREAK,
						get_next_char);
				sp_cls = c_cls;
				posLast = posCurSt;
			}
			break;

	    case WBP_MidLetter:
			if (p_cls == WBP_ALetter) /* WBP6,7 */
			{
				/* Go on */
			}
			else
			{
				set_brks_to(s, brks, posLast, posCurSt, len, WORDBREAK_BREAK,
						get_next_char);
				sp_cls = c_cls;
				posLast = posCurSt;
			}
			break;

	    case WBP_MidNum:
			if (p_cls == WBP_Numeric) /* WBP11,12 */
			{
				/* Go on */
			}
			else
			{
				set_brks_to(s, brks, posLast, posCurSt, len, WORDBREAK_BREAK,
						get_next_char);
				sp_cls = c_cls;
				posLast = posCurSt;
			}
			break;

	    case WBP_Numeric:
			if ((sp_cls == WBP_Numeric) || /* WB8,11,12 */
					((sp_cls == WBP_ALetter) && (p_cls == WBP_ALetter)) || /* WB9 */
					(sp_cls == WBP_ExtendNumLet)) /* WB13b */
			{
				set_brks_to(s, brks, posLast, posCurSt, len, WORDBREAK_NOBREAK,
						get_next_char);
			}
			/* No rule found, reset */
			else
			{
				set_brks_to(s, brks, posLast, posCurSt, len, WORDBREAK_BREAK,
						get_next_char);
			}
			sp_cls = c_cls;
			posLast = posCurSt;
			break;

	    case WBP_ExtendNumLet:
			/* WB13a,13b */
			if ((sp_cls == p_cls) &&
				((p_cls == WBP_ALetter) ||
				 (p_cls == WBP_Numeric) ||
				 (p_cls == WBP_Katakana) ||
				 (p_cls == WBP_ExtendNumLet)))
			{
				set_brks_to(s, brks, posLast, posCurSt, len, WORDBREAK_NOBREAK,
						get_next_char);
			}
			/* No rule found, reset */
			else
			{
				set_brks_to(s, brks, posLast, posCurSt, len, WORDBREAK_BREAK,
						get_next_char);
			}
			sp_cls = c_cls;
			posLast = posCurSt;
			break;

		 case WBP_Any:
			/* Allow breaks and reset */
			set_brks_to(s, brks, posLast, posCurSt, len, WORDBREAK_BREAK,
					get_next_char);
			sp_cls = c_cls;
			posLast = posCurSt;
			break;

	    default:
			/* Error, should never get here! */
			assert(0);
			break;
		}

		p_cls = c_cls;
		posCurSt = posCur;
		ch = get_next_char(s, len, &posCur);
    }

	/* WB2 */
	set_brks_to(s, brks, posLast, posCur, len, WORDBREAK_BREAK,
			get_next_char);
}

/**
 * Sets the word breaking information for a UTF-8 input string.
 *
 * @param[in]  s	input UTF-8 string
 * @param[in]  len	length of the input
 * @param[in]  lang	language of the input
 * @param[out] brks	pointer to the output breaking data, containing
 *					#WORDBREAK_BREAK, #WORDBREAK_NOBREAK, or
 *					#WORDBREAK_INSIDEACHAR
 */
void set_wordbreaks_utf8(
		const utf8_t *s,
		size_t len,
		const char *lang,
		char *brks)
{
	set_wordbreaks(s, len, lang, brks,
				   (get_next_char_t)lb_get_next_char_utf8);
}

/**
 * Sets the word breaking information for a UTF-16 input string.
 *
 * @param[in]  s	input UTF-16 string
 * @param[in]  len	length of the input
 * @param[in]  lang	language of the input
 * @param[out] brks	pointer to the output breaking data, containing
 *					#WORDBREAK_BREAK, #WORDBREAK_NOBREAK, or
 *					#WORDBREAK_INSIDEACHAR
 */
void set_wordbreaks_utf16(
		const utf16_t *s,
		size_t len,
		const char *lang,
		char *brks)
{
	set_wordbreaks(s, len, lang, brks,
				   (get_next_char_t)lb_get_next_char_utf16);
}

/**
 * Sets the word breaking information for a UTF-32 input string.
 *
 * @param[in]  s	input UTF-32 string
 * @param[in]  len	length of the input
 * @param[in]  lang	language of the input
 * @param[out] brks	pointer to the output breaking data, containing
 *					#WORDBREAK_BREAK, #WORDBREAK_NOBREAK, or
 *					#WORDBREAK_INSIDEACHAR
 */
void set_wordbreaks_utf32(
		const utf32_t *s,
		size_t len,
		const char *lang,
		char *brks)
{
	set_wordbreaks(s, len, lang, brks,
				   (get_next_char_t)lb_get_next_char_utf32);
}
