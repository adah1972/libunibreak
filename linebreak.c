/* vim: set tabstop=4 shiftwidth=4: */

/*
 * Line breaking in a Unicode sequence.  Designed to be used in a
 * generic text renderer.
 *
 * Copyright (C) 2008 Wu Yongwei <wuyongwei at gmail dot com>
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
 * The main reference is Unicode 5.0.0 Standard Annex 14, Revision 19,
 * available at
 *		<URL:http://www.unicode.org/reports/tr14/tr14-19.html>
 *
 * The Unicode Terms of Use are available at
 *		<URL:http://www.unicode.org/copyright.html>
 */

/**
 * @file	linebreak.c
 *
 * Implementation of the line breaking algorithm as described in Unicode
 * 5.0.0 Standard Annex 14.
 *
 * @version	0.6, 2008/02/23
 * @author	Wu Yongwei
 */

#include <assert.h>
#include <stddef.h>
#include <string.h>
#include "linebreak.h"

/**
 * Constant value to mark the end of string.  It is not a valid Unicode
 * character.
 */
#define EOS 0xFFFF

/**
 * Line break classes.  This is a direct mapping of Table 1 of Unicode
 * Standard Annex 14, Revision 19.
 */
enum LineBreakClass
{
	/* This is used to signal an error condition. */
	LBP_Undefined,	/**< Undefined */

	/* The following break classes are treated in the pair table. */
	LBP_OP,			/**< Opening punctuation */
	LBP_CL,			/**< Closing punctuation */
	LBP_QU,			/**< Ambiguous quotation */
	LBP_GL,			/**< Glue */
	LBP_NS,			/**< Non-starters */
	LBP_EX,			/**< Exclamation/Interrogation */
	LBP_SY,			/**< Symbols allowing break after */
	LBP_IS,			/**< Infix separator */
	LBP_PR,			/**< Prefix */
	LBP_PO,			/**< Postfix */
	LBP_NU,			/**< Numeric */
	LBP_AL,			/**< Alphabetic */
	LBP_ID,			/**< Ideographic */
	LBP_IN,			/**< Inseparable characters */
	LBP_HY,			/**< Hyphen */
	LBP_BA,			/**< Break after */
	LBP_BB,			/**< Break before */
	LBP_B2,			/**< Break on either side (but not pair) */
	LBP_ZW,			/**< Zero-width space */
	LBP_CM,			/**< Combining marks */
	LBP_WJ,			/**< Word joiner */
	LBP_H2,			/**< Hangul LV */
	LBP_H3,			/**< Hangul LVT */
	LBP_JL,			/**< Hangul L Jamo */
	LBP_JV,			/**< Hangul V Jamo */
	LBP_JT,			/**< Hangul T Jamo */

	/* The following break classes are not treated in the pair table */
	LBP_AI,			/**< Ambiguous (alphabetic or ideograph) */
	LBP_BK,			/**< Break (mandatory) */
	LBP_CB,			/**< Contingent break */
	LBP_CR,			/**< Carriage return */
	LBP_LF,			/**< Line feed */
	LBP_NL,			/**< Next line */
	LBP_SA,			/**< South-East Asian */
	LBP_SG,			/**< Surrogates */
	LBP_SP,			/**< Space */
	LBP_XX			/**< Unknown */
};

/**
 * Enumeration of break actions.  They are used in the break action
 * pair table below.
 */
enum BreakAction
{
	DIRECT_BRK,		/**< Direct break opportunity */
	INDRCT_BRK,		/**< Indirect break opportunity */
	CM_INDRCT_BRK,	/**< Indirect break opportunity for combining marks */
	CM_PROHIBTD_BRK,/**< Prohibited break for combining marks */
	PROHIBTD_BRK	/**< Prohibited break */
};

/**
 * Break action pair table.
 */
static enum BreakAction baTable[LBP_JT][LBP_JT] = {
	{	/* OP */
		PROHIBTD_BRK, PROHIBTD_BRK, PROHIBTD_BRK, PROHIBTD_BRK,
		PROHIBTD_BRK, PROHIBTD_BRK, PROHIBTD_BRK, PROHIBTD_BRK,
		PROHIBTD_BRK, PROHIBTD_BRK, PROHIBTD_BRK, PROHIBTD_BRK,
		PROHIBTD_BRK, PROHIBTD_BRK, PROHIBTD_BRK, PROHIBTD_BRK,
		PROHIBTD_BRK, PROHIBTD_BRK, PROHIBTD_BRK, CM_PROHIBTD_BRK,
		PROHIBTD_BRK, PROHIBTD_BRK, PROHIBTD_BRK, PROHIBTD_BRK,
		PROHIBTD_BRK, PROHIBTD_BRK },
	{	/* CL */
		DIRECT_BRK, PROHIBTD_BRK, INDRCT_BRK, INDRCT_BRK,
		PROHIBTD_BRK, PROHIBTD_BRK, PROHIBTD_BRK, PROHIBTD_BRK,
		INDRCT_BRK, INDRCT_BRK, INDRCT_BRK, INDRCT_BRK,
		DIRECT_BRK, DIRECT_BRK, INDRCT_BRK, INDRCT_BRK,
		DIRECT_BRK, DIRECT_BRK, PROHIBTD_BRK, CM_INDRCT_BRK,
		PROHIBTD_BRK, DIRECT_BRK, DIRECT_BRK, DIRECT_BRK,
		DIRECT_BRK, DIRECT_BRK },
	{	/* QU */
		PROHIBTD_BRK, PROHIBTD_BRK, INDRCT_BRK, INDRCT_BRK,
		INDRCT_BRK, PROHIBTD_BRK, PROHIBTD_BRK, PROHIBTD_BRK,
		INDRCT_BRK, INDRCT_BRK, INDRCT_BRK, INDRCT_BRK,
		INDRCT_BRK, INDRCT_BRK, INDRCT_BRK, INDRCT_BRK,
		INDRCT_BRK, INDRCT_BRK, PROHIBTD_BRK, CM_INDRCT_BRK,
		PROHIBTD_BRK, INDRCT_BRK, INDRCT_BRK, INDRCT_BRK,
		INDRCT_BRK, INDRCT_BRK },
	{	/* GL */
		INDRCT_BRK, PROHIBTD_BRK, INDRCT_BRK, INDRCT_BRK,
		INDRCT_BRK, PROHIBTD_BRK, PROHIBTD_BRK, PROHIBTD_BRK,
		INDRCT_BRK, INDRCT_BRK, INDRCT_BRK, INDRCT_BRK,
		INDRCT_BRK, INDRCT_BRK, INDRCT_BRK, INDRCT_BRK,
		INDRCT_BRK, INDRCT_BRK, PROHIBTD_BRK, CM_INDRCT_BRK,
		PROHIBTD_BRK, INDRCT_BRK, INDRCT_BRK, INDRCT_BRK,
		INDRCT_BRK, INDRCT_BRK },
	{	/* NS */
		DIRECT_BRK, PROHIBTD_BRK, INDRCT_BRK, INDRCT_BRK,
		INDRCT_BRK, PROHIBTD_BRK, PROHIBTD_BRK, PROHIBTD_BRK,
		DIRECT_BRK, DIRECT_BRK, DIRECT_BRK, DIRECT_BRK,
		DIRECT_BRK, DIRECT_BRK, INDRCT_BRK, INDRCT_BRK,
		DIRECT_BRK, DIRECT_BRK, PROHIBTD_BRK, CM_INDRCT_BRK,
		PROHIBTD_BRK, DIRECT_BRK, DIRECT_BRK, DIRECT_BRK,
		DIRECT_BRK, DIRECT_BRK },
	{	/* EX */
		DIRECT_BRK, PROHIBTD_BRK, INDRCT_BRK, INDRCT_BRK,
		INDRCT_BRK, PROHIBTD_BRK, PROHIBTD_BRK, PROHIBTD_BRK,
		DIRECT_BRK, DIRECT_BRK, DIRECT_BRK, DIRECT_BRK,
		DIRECT_BRK, DIRECT_BRK, INDRCT_BRK, INDRCT_BRK,
		DIRECT_BRK, DIRECT_BRK, PROHIBTD_BRK, CM_INDRCT_BRK,
		PROHIBTD_BRK, DIRECT_BRK, DIRECT_BRK, DIRECT_BRK,
		DIRECT_BRK, DIRECT_BRK },
	{	/* SY */
		DIRECT_BRK, PROHIBTD_BRK, INDRCT_BRK, INDRCT_BRK,
		INDRCT_BRK, PROHIBTD_BRK, PROHIBTD_BRK, PROHIBTD_BRK,
		DIRECT_BRK, DIRECT_BRK, INDRCT_BRK, DIRECT_BRK,
		DIRECT_BRK, DIRECT_BRK, INDRCT_BRK, INDRCT_BRK,
		DIRECT_BRK, DIRECT_BRK, PROHIBTD_BRK, CM_INDRCT_BRK,
		PROHIBTD_BRK, DIRECT_BRK, DIRECT_BRK, DIRECT_BRK,
		DIRECT_BRK, DIRECT_BRK },
	{	/* IS */
		DIRECT_BRK, PROHIBTD_BRK, INDRCT_BRK, INDRCT_BRK,
		INDRCT_BRK, PROHIBTD_BRK, PROHIBTD_BRK, PROHIBTD_BRK,
		DIRECT_BRK, DIRECT_BRK, INDRCT_BRK, INDRCT_BRK,
		DIRECT_BRK, DIRECT_BRK, INDRCT_BRK, INDRCT_BRK,
		DIRECT_BRK, DIRECT_BRK, PROHIBTD_BRK, CM_INDRCT_BRK,
		PROHIBTD_BRK, DIRECT_BRK, DIRECT_BRK, DIRECT_BRK,
		DIRECT_BRK, DIRECT_BRK },
	{	/* PR */
		INDRCT_BRK, PROHIBTD_BRK, INDRCT_BRK, INDRCT_BRK,
		INDRCT_BRK, PROHIBTD_BRK, PROHIBTD_BRK, PROHIBTD_BRK,
		DIRECT_BRK, DIRECT_BRK, INDRCT_BRK, INDRCT_BRK,
		INDRCT_BRK, DIRECT_BRK, INDRCT_BRK, INDRCT_BRK,
		DIRECT_BRK, DIRECT_BRK, PROHIBTD_BRK, CM_INDRCT_BRK,
		PROHIBTD_BRK, INDRCT_BRK, INDRCT_BRK, INDRCT_BRK,
		INDRCT_BRK, INDRCT_BRK },
	{	/* PO */
		INDRCT_BRK, PROHIBTD_BRK, INDRCT_BRK, INDRCT_BRK,
		INDRCT_BRK, PROHIBTD_BRK, PROHIBTD_BRK, PROHIBTD_BRK,
		DIRECT_BRK, DIRECT_BRK, INDRCT_BRK, INDRCT_BRK,
		DIRECT_BRK, DIRECT_BRK, INDRCT_BRK, INDRCT_BRK,
		DIRECT_BRK, DIRECT_BRK, PROHIBTD_BRK, CM_INDRCT_BRK,
		PROHIBTD_BRK, DIRECT_BRK, DIRECT_BRK, DIRECT_BRK,
		DIRECT_BRK, DIRECT_BRK },
	{	/* NU */
		INDRCT_BRK, PROHIBTD_BRK, INDRCT_BRK, INDRCT_BRK,
		INDRCT_BRK, PROHIBTD_BRK, PROHIBTD_BRK, PROHIBTD_BRK,
		INDRCT_BRK, INDRCT_BRK, INDRCT_BRK, INDRCT_BRK,
		DIRECT_BRK, INDRCT_BRK, INDRCT_BRK, INDRCT_BRK,
		DIRECT_BRK, DIRECT_BRK, PROHIBTD_BRK, CM_INDRCT_BRK,
		PROHIBTD_BRK, DIRECT_BRK, DIRECT_BRK, DIRECT_BRK,
		DIRECT_BRK, DIRECT_BRK },
	{	/* AL */
		INDRCT_BRK, PROHIBTD_BRK, INDRCT_BRK, INDRCT_BRK,
		INDRCT_BRK, PROHIBTD_BRK, PROHIBTD_BRK, PROHIBTD_BRK,
		DIRECT_BRK, DIRECT_BRK, INDRCT_BRK, INDRCT_BRK,
		DIRECT_BRK, INDRCT_BRK, INDRCT_BRK, INDRCT_BRK,
		DIRECT_BRK, DIRECT_BRK, PROHIBTD_BRK, CM_INDRCT_BRK,
		PROHIBTD_BRK, DIRECT_BRK, DIRECT_BRK, DIRECT_BRK,
		DIRECT_BRK, DIRECT_BRK },
	{	/* ID */
		DIRECT_BRK, PROHIBTD_BRK, INDRCT_BRK, INDRCT_BRK,
		INDRCT_BRK, PROHIBTD_BRK, PROHIBTD_BRK, PROHIBTD_BRK,
		DIRECT_BRK, INDRCT_BRK, DIRECT_BRK, DIRECT_BRK,
		DIRECT_BRK, INDRCT_BRK, INDRCT_BRK, INDRCT_BRK,
		DIRECT_BRK, DIRECT_BRK, PROHIBTD_BRK, CM_INDRCT_BRK,
		PROHIBTD_BRK, DIRECT_BRK, DIRECT_BRK, DIRECT_BRK,
		DIRECT_BRK, DIRECT_BRK },
	{	/* IN */
		DIRECT_BRK, PROHIBTD_BRK, INDRCT_BRK, INDRCT_BRK,
		INDRCT_BRK, PROHIBTD_BRK, PROHIBTD_BRK, PROHIBTD_BRK,
		DIRECT_BRK, DIRECT_BRK, DIRECT_BRK, DIRECT_BRK,
		DIRECT_BRK, INDRCT_BRK, INDRCT_BRK, INDRCT_BRK,
		DIRECT_BRK, DIRECT_BRK, PROHIBTD_BRK, CM_INDRCT_BRK,
		PROHIBTD_BRK, DIRECT_BRK, DIRECT_BRK, DIRECT_BRK,
		DIRECT_BRK, DIRECT_BRK },
	{	/* HY */
		DIRECT_BRK, PROHIBTD_BRK, INDRCT_BRK, INDRCT_BRK,
		INDRCT_BRK, PROHIBTD_BRK, PROHIBTD_BRK, PROHIBTD_BRK,
		DIRECT_BRK, DIRECT_BRK, INDRCT_BRK, DIRECT_BRK,
		DIRECT_BRK, DIRECT_BRK, INDRCT_BRK, INDRCT_BRK,
		DIRECT_BRK, DIRECT_BRK, PROHIBTD_BRK, CM_INDRCT_BRK,
		PROHIBTD_BRK, DIRECT_BRK, DIRECT_BRK, DIRECT_BRK,
		DIRECT_BRK, DIRECT_BRK },
	{	/* BA */
		DIRECT_BRK, PROHIBTD_BRK, INDRCT_BRK, INDRCT_BRK,
		INDRCT_BRK, PROHIBTD_BRK, PROHIBTD_BRK, PROHIBTD_BRK,
		DIRECT_BRK, DIRECT_BRK, DIRECT_BRK, DIRECT_BRK,
		DIRECT_BRK, DIRECT_BRK, INDRCT_BRK, INDRCT_BRK,
		DIRECT_BRK, DIRECT_BRK, PROHIBTD_BRK, CM_INDRCT_BRK,
		PROHIBTD_BRK, DIRECT_BRK, DIRECT_BRK, DIRECT_BRK,
		DIRECT_BRK, DIRECT_BRK },
	{	/* BB */
		INDRCT_BRK, PROHIBTD_BRK, INDRCT_BRK, INDRCT_BRK,
		INDRCT_BRK, PROHIBTD_BRK, PROHIBTD_BRK, PROHIBTD_BRK,
		INDRCT_BRK, INDRCT_BRK, INDRCT_BRK, INDRCT_BRK,
		INDRCT_BRK, INDRCT_BRK, INDRCT_BRK, INDRCT_BRK,
		INDRCT_BRK, INDRCT_BRK, PROHIBTD_BRK, CM_INDRCT_BRK,
		PROHIBTD_BRK, INDRCT_BRK, INDRCT_BRK, INDRCT_BRK,
		INDRCT_BRK, INDRCT_BRK },
	{	/* B2 */
		DIRECT_BRK, PROHIBTD_BRK, INDRCT_BRK, INDRCT_BRK,
		INDRCT_BRK, PROHIBTD_BRK, PROHIBTD_BRK, PROHIBTD_BRK,
		DIRECT_BRK, DIRECT_BRK, DIRECT_BRK, DIRECT_BRK,
		DIRECT_BRK, DIRECT_BRK, INDRCT_BRK, INDRCT_BRK,
		DIRECT_BRK, PROHIBTD_BRK, PROHIBTD_BRK, CM_INDRCT_BRK,
		PROHIBTD_BRK, DIRECT_BRK, DIRECT_BRK, DIRECT_BRK,
		DIRECT_BRK, DIRECT_BRK },
	{	/* ZW */
		DIRECT_BRK, DIRECT_BRK, DIRECT_BRK, DIRECT_BRK,
		DIRECT_BRK, DIRECT_BRK, DIRECT_BRK, DIRECT_BRK,
		DIRECT_BRK, DIRECT_BRK, DIRECT_BRK, DIRECT_BRK,
		DIRECT_BRK, DIRECT_BRK, DIRECT_BRK, DIRECT_BRK,
		DIRECT_BRK, DIRECT_BRK, PROHIBTD_BRK, DIRECT_BRK,
		DIRECT_BRK, DIRECT_BRK, DIRECT_BRK, DIRECT_BRK,
		DIRECT_BRK, DIRECT_BRK },
	{	/* CM */
		DIRECT_BRK, PROHIBTD_BRK, INDRCT_BRK, INDRCT_BRK,
		INDRCT_BRK, PROHIBTD_BRK, PROHIBTD_BRK, PROHIBTD_BRK,
		DIRECT_BRK, DIRECT_BRK, INDRCT_BRK, INDRCT_BRK,
		DIRECT_BRK, INDRCT_BRK, INDRCT_BRK, INDRCT_BRK,
		DIRECT_BRK, DIRECT_BRK, PROHIBTD_BRK, CM_INDRCT_BRK,
		PROHIBTD_BRK, DIRECT_BRK, DIRECT_BRK, DIRECT_BRK,
		DIRECT_BRK, DIRECT_BRK },
	{	/* WJ */
		INDRCT_BRK, PROHIBTD_BRK, INDRCT_BRK, INDRCT_BRK,
		INDRCT_BRK, PROHIBTD_BRK, PROHIBTD_BRK, PROHIBTD_BRK,
		INDRCT_BRK, INDRCT_BRK, INDRCT_BRK, INDRCT_BRK,
		INDRCT_BRK, INDRCT_BRK, INDRCT_BRK, INDRCT_BRK,
		INDRCT_BRK, INDRCT_BRK, PROHIBTD_BRK, CM_INDRCT_BRK,
		PROHIBTD_BRK, INDRCT_BRK, INDRCT_BRK, INDRCT_BRK,
		INDRCT_BRK, INDRCT_BRK },
	{	/* H2 */
		DIRECT_BRK, PROHIBTD_BRK, INDRCT_BRK, INDRCT_BRK,
		INDRCT_BRK, PROHIBTD_BRK, PROHIBTD_BRK, PROHIBTD_BRK,
		DIRECT_BRK, INDRCT_BRK, DIRECT_BRK, DIRECT_BRK,
		DIRECT_BRK, INDRCT_BRK, INDRCT_BRK, INDRCT_BRK,
		DIRECT_BRK, DIRECT_BRK, PROHIBTD_BRK, CM_INDRCT_BRK,
		PROHIBTD_BRK, DIRECT_BRK, DIRECT_BRK, DIRECT_BRK,
		INDRCT_BRK, INDRCT_BRK },
	{	/* H3 */
		DIRECT_BRK, PROHIBTD_BRK, INDRCT_BRK, INDRCT_BRK,
		INDRCT_BRK, PROHIBTD_BRK, PROHIBTD_BRK, PROHIBTD_BRK,
		DIRECT_BRK, INDRCT_BRK, DIRECT_BRK, DIRECT_BRK,
		DIRECT_BRK, INDRCT_BRK, INDRCT_BRK, INDRCT_BRK,
		DIRECT_BRK, DIRECT_BRK, PROHIBTD_BRK, CM_INDRCT_BRK,
		PROHIBTD_BRK, DIRECT_BRK, DIRECT_BRK, DIRECT_BRK,
		DIRECT_BRK, INDRCT_BRK },
	{	/* JL */
		DIRECT_BRK, PROHIBTD_BRK, INDRCT_BRK, INDRCT_BRK,
		INDRCT_BRK, PROHIBTD_BRK, PROHIBTD_BRK, PROHIBTD_BRK,
		DIRECT_BRK, INDRCT_BRK, DIRECT_BRK, DIRECT_BRK,
		DIRECT_BRK, INDRCT_BRK, INDRCT_BRK, INDRCT_BRK,
		DIRECT_BRK, DIRECT_BRK, PROHIBTD_BRK, CM_INDRCT_BRK,
		PROHIBTD_BRK, INDRCT_BRK, INDRCT_BRK, INDRCT_BRK,
		INDRCT_BRK, DIRECT_BRK },
	{	/* JV */
		DIRECT_BRK, PROHIBTD_BRK, INDRCT_BRK, INDRCT_BRK,
		INDRCT_BRK, PROHIBTD_BRK, PROHIBTD_BRK, PROHIBTD_BRK,
		DIRECT_BRK, INDRCT_BRK, DIRECT_BRK, DIRECT_BRK,
		DIRECT_BRK, INDRCT_BRK, INDRCT_BRK, INDRCT_BRK,
		DIRECT_BRK, DIRECT_BRK, PROHIBTD_BRK, CM_INDRCT_BRK,
		PROHIBTD_BRK, DIRECT_BRK, DIRECT_BRK, DIRECT_BRK,
		INDRCT_BRK, INDRCT_BRK },
	{	/* JT */
		DIRECT_BRK, PROHIBTD_BRK, INDRCT_BRK, INDRCT_BRK,
		INDRCT_BRK, PROHIBTD_BRK, PROHIBTD_BRK, PROHIBTD_BRK,
		DIRECT_BRK, INDRCT_BRK, DIRECT_BRK, DIRECT_BRK,
		DIRECT_BRK, INDRCT_BRK, INDRCT_BRK, INDRCT_BRK,
		DIRECT_BRK, DIRECT_BRK, PROHIBTD_BRK, CM_INDRCT_BRK,
		PROHIBTD_BRK, DIRECT_BRK, DIRECT_BRK, DIRECT_BRK,
		DIRECT_BRK, INDRCT_BRK }
};

/**
 * Struct for entries of line break properties.
 */
struct LineBreakProperties
{
	utf32_t start;				/**< Starting coding point */
	utf32_t end;				/**< End coding point */
	enum LineBreakClass prop;	/**< The line break property */
};

#include "linebreakdata.c"

/**
 * English-specifc data over the default Unicode rules.
 */
static struct LineBreakProperties lbpEnglish[] = {
	{ 0x2B,   0x2B,   LBP_AL },	/* Plus sign: no break inside "C++" */
	{ 0x2F,   0x2F,   LBP_AL },	/* Solidus: no break inside "w/o" */
	{ 0x2018, 0x2018, LBP_OP },	/* Left single quotation mark: opening */
	{ 0x2019, 0x2019, LBP_CL },	/* Right single quotation mark: closing */
	{ 0x201C, 0x201C, LBP_OP },	/* Left double quotation mark: opening */
	{ 0x201D, 0x201D, LBP_CL },	/* Right double quotation mark: closing */
	{ 0, 0, LBP_Undefined }
};

/**
 * Chinese-specifc data over the default Unicode rules.
 */
static struct LineBreakProperties lbpChinese[] = {
	{ 0x2018, 0x2018, LBP_OP },	/* Left single quotation mark: opening */
	{ 0x2019, 0x2019, LBP_CL },	/* Right single quotation mark: closing */
	{ 0x201C, 0x201C, LBP_OP },	/* Left double quotation mark: opening */
	{ 0x201D, 0x201D, LBP_CL },	/* Right double quotation mark: closing */
	{ 0, 0, LBP_Undefined }
};

/**
 * Struct for association of language-specific line breaking properties
 * with language names.
 */
struct LineBreakPropertiesLang
{
	const char *lang;					/**< Language name */
	size_t namelen;						/**< Length of name to match */
	struct LineBreakProperties *lbp;	/**< Pointer to associated data */
};

/**
 * Association data of language-specific line breaking properties with
 * language names.
 */
struct LineBreakPropertiesLang lbpLangs[] = {
	{ "en", 2, lbpEnglish },
	{ "zh", 2, lbpChinese },
	{ NULL, 0, lbpDefault }
};

/**
 * Gets the line breaking property of a character.
 *
 * @param ch	character to check
 * @param lbp	line breaking property array
 * @return		the line breaking class found; \c LBP_XX otherwise
 */
static enum LineBreakClass get_char_lb_class(
		utf32_t ch,
		struct LineBreakProperties *lbp)
{
	while (lbp->prop != LBP_Undefined)
	{
		if (ch >= lbp->start && ch <= lbp->end)
			return lbp->prop;
		++lbp;
	}
	return LBP_XX;
}

/**
 * Gets the line breaking property of a character for a specific
 * language.  This function will check the language-specific data first,
 * and then the default data if there is no language-specific property
 * available for the character.
 *
 * @param ch	character to check
 * @param lbp	the language context
 * @return		the line breaking class found; \c LBP_XX otherwise
 */
static enum LineBreakClass get_char_lb_class_lang(
		utf32_t ch,
		const char *lang)
{
	struct LineBreakPropertiesLang *lbplIter;
	struct LineBreakProperties *lbpPrimary;
	enum LineBreakClass lbcResult;

	lbpPrimary = NULL;
	if (lang != NULL)
	{
		for (lbplIter = lbpLangs; lbplIter->lang != NULL; ++lbplIter)
		{
			if (strncmp(lang, lbplIter->lang, lbplIter->namelen) == 0)
			{
				lbpPrimary = lbplIter->lbp;
				break;
			}
		}
	}

	if (lbpPrimary)
	{
		lbcResult = get_char_lb_class(ch, lbpPrimary);
		if (lbcResult != LBP_XX)
			return lbcResult;
	}
	return get_char_lb_class(ch, lbpDefault);
}

/**
 * Resolves the line breaking class for certain ambiguous or complicated
 * characters.  Currently they are treated in a simplistic way.
 *
 * @param lbc	the line breaking class to resolve
 * @param lang	the language context
 * @return		the resolved line breaking class
 */
static enum LineBreakClass resolve_lb_class(
		enum LineBreakClass lbc,
		const char *lang
#ifdef __GNUC__
		__attribute__((unused))
#endif
)
{
	switch (lbc)
	{
	case LBP_AI:
	case LBP_SA:
	case LBP_SG:
	case LBP_XX:
		return LBP_AL;
	default:
		return lbc;
	}
}

typedef utf32_t (*get_next_char_t)(const void *, size_t, size_t *);

static utf32_t get_next_char_utf8(
		const utf8_t *s,
		size_t len,
		size_t *ip)
{
	utf8_t ch;
	utf32_t res;
	assert(*ip <= len);
	if (*ip == len)
		return EOS;
	ch = s[(*ip)++];
	if (ch < 0xC2 || ch > 0xF4)
	{	/* One-byte sequence, tail (should not occur), or invalid */
		return ch;
	}
	else if (ch < 0xE0)
	{	/* Two-byte sequence */
		if (*ip == len)
			return EOS;
		res = ((ch & 0x1F) << 6) + (s[*ip] & 0x3F);
		++(*ip);
		return res;
	}
	else if (ch < 0xF0)
	{	/* Three-byte sequence */
		if (*ip + 1 >= len)
			return EOS;
		res = ((ch & 0x0F) << 12) +
			  ((s[*ip] & 0x3F) << 6) +
			  (s[*ip + 1] & 0x3F);
		*ip += 2;
		return res;
	}
	else
	{	/* Four-byte sequence */
		if (*ip + 2 >= len)
			return EOS;
		res = ((ch & 0x07) << 18) +
			  ((s[*ip] & 0x3F) << 12) +
			  ((s[*ip + 1] & 0x3F) << 12) +
			  (s[*ip + 2] & 0x3F);
		*ip += 3;
		return res;
	}
}

static utf32_t get_next_char_utf16(
		const utf16_t *s,
		size_t len,
		size_t *ip)
{
	utf16_t ch;
	assert(*ip <= len);
	if (*ip == len)
		return EOS;
	ch = s[(*ip)++];
	if (ch < 0xD800 || ch > 0xDBFF || s[*ip] < 0xDC00 || s[*ip] > 0xDFFF)
		return ch;
	if (*ip == len)
		return EOS;
	return (((utf32_t)ch & 0x3FF) << 10) + (s[(*ip)++] & 0x3FF) + 0x10000;
}

static utf32_t get_next_char_utf32(
		const utf32_t *s,
		size_t len,
		size_t *ip)
{
	assert(*ip <= len);
	if (*ip == len)
		return EOS;
	return s[(*ip)++];
}

/**
 * Sets the line breaking information for a generic input string.
 *
 * @param s		the input string
 * @param len	length of the input
 * @param lang	language of the input
 * @param brks	pointer to the output breaking data, containing \c
 * 				LINEBREAK_MUSTBREAK, \c LINEBREAK_ALLOWBREAK, \c
 * 				LINEBREAK_NOBREAK, or \c LINEBREAK_INSIDEACHAR
 * @param get_next_char	function to get the next UTF-32 character
 */
static void set_linebreaks(
		const void *s,
		size_t len,
		const char *lang,
		char *brks,
		get_next_char_t get_next_char)
{
	utf32_t ch;
	enum LineBreakClass lbcCur;
	enum LineBreakClass lbcNew;
	enum LineBreakClass lbcLast;
	size_t posCur = 0;
	size_t posLast = 0;

	--posLast;	/* To be ++'d later */
	ch = get_next_char(s, len, &posCur);
	if (ch == EOS)
		return;
	lbcCur = resolve_lb_class(get_char_lb_class_lang(ch, lang), lang);
	lbcNew = LBP_Undefined;

nextline:

	/* Special treatment for the first character */
	switch (lbcCur)
	{
	case LBP_LF:
	case LBP_NL:
		lbcCur = LBP_BK;
		break;
	case LBP_SP:
		lbcCur = LBP_WJ;
		break;
	default:
		break;
	}

	/* Process a line till an explicit break or end of string */
	for (;;)
	{
		for (++posLast; posLast < posCur - 1; ++posLast)
		{
			brks[posLast] = LINEBREAK_INSIDEACHAR;
		}
		assert(posLast == posCur - 1);
		lbcLast = lbcNew;
		ch = get_next_char(s, len, &posCur);
		if (ch == EOS)
			break;
		lbcNew = get_char_lb_class_lang(ch, lang);
		if (lbcCur == LBP_BK || (lbcCur == LBP_CR && lbcNew != LBP_LF))
		{
			brks[posLast] = LINEBREAK_MUSTBREAK;
			lbcCur = resolve_lb_class(lbcNew, lang);
			goto nextline;
		}

		switch (lbcNew)
		{
		case LBP_SP:
			brks[posLast] = LINEBREAK_NOBREAK;
			continue;
		case LBP_BK:
		case LBP_LF:
		case LBP_NL:
			brks[posLast] = LINEBREAK_NOBREAK;
			lbcCur = LBP_BK;
			continue;
		case LBP_CR:
			brks[posLast] = LINEBREAK_NOBREAK;
			lbcCur = LBP_CR;
			continue;
		case LBP_CB:
			brks[posLast] = LINEBREAK_ALLOWBREAK;
			lbcCur = LBP_BA;
			continue;
		default:
			break;
		}

		lbcNew = resolve_lb_class(lbcNew, lang);

		assert(lbcCur <= LBP_JT);
		assert(lbcNew <= LBP_JT);
		switch (baTable[lbcCur - 1][lbcNew - 1])
		{
		case DIRECT_BRK:
			brks[posLast] = LINEBREAK_ALLOWBREAK;
			break;
		case CM_INDRCT_BRK:
		case INDRCT_BRK:
			if (lbcLast == LBP_SP)
			{
				brks[posLast] = LINEBREAK_ALLOWBREAK;
			}
			else
			{
				brks[posLast] = LINEBREAK_NOBREAK;
			}
			break;
		case CM_PROHIBTD_BRK:
			brks[posLast] = LINEBREAK_NOBREAK;
			if (lbcLast != LBP_SP)
				continue;
			break;
		case PROHIBTD_BRK:
			brks[posLast] = LINEBREAK_NOBREAK;
			break;
		}

		lbcCur = lbcNew;
	}

	assert(posLast == posCur - 1 && posCur <= len);
	/* Break after the last character */
	brks[posLast] = LINEBREAK_MUSTBREAK;
	/* When the input contains incomplete sequences */
	while (posCur < len)
	{
		brks[posCur++] == LINEBREAK_INSIDEACHAR;
	}
}

/**
 * Sets the line breaking information for a UTF-8 input string.
 *
 * @param s		the input string
 * @param len	length of the input
 * @param lang	language of the input
 * @param brks	pointer to the output breaking data, containing \c
 * 				LINEBREAK_MUSTBREAK, \c LINEBREAK_ALLOWBREAK, \c
 * 				LINEBREAK_NOBREAK, or \c LINEBREAK_INSIDEACHAR
 */
void set_linebreaks_utf8(
		const utf8_t *s,
		size_t len,
		const char *lang,
		char *brks)
{
	set_linebreaks(s, len, lang, brks, (get_next_char_t)get_next_char_utf8);
}

/**
 * Sets the line breaking information for a UTF-16 input string.
 *
 * @param s		the input string
 * @param len	length of the input
 * @param lang	language of the input
 * @param brks	pointer to the output breaking data, containing \c
 * 				LINEBREAK_MUSTBREAK, \c LINEBREAK_ALLOWBREAK, \c
 * 				LINEBREAK_NOBREAK, or \c LINEBREAK_INSIDEACHAR
 */
void set_linebreaks_utf16(
		const utf16_t *s,
		size_t len,
		const char *lang,
		char *brks)
{
	set_linebreaks(s, len, lang, brks, (get_next_char_t)get_next_char_utf16);
}

/**
 * Sets the line breaking information for a UTF-32 input string.
 *
 * @param s		the input string
 * @param len	length of the input
 * @param lang	language of the input
 * @param brks	pointer to the output breaking data, containing \c
 * 				LINEBREAK_MUSTBREAK, \c LINEBREAK_ALLOWBREAK, \c
 * 				LINEBREAK_NOBREAK, or \c LINEBREAK_INSIDEACHAR
 */
void set_linebreaks_utf32(
		const utf32_t *s,
		size_t len,
		const char *lang,
		char *brks)
{
	set_linebreaks(s, len, lang, brks, (get_next_char_t)get_next_char_utf32);
}

/**
 * Tells whether a line break can occur between two Unicode characters.
 * This is a wrapper function to expose a simple interface.  It is
 * better to use set_linebreaks_utf32 instead, since complicated cases
 * involving combining marks, spaces, etc. cannot be correctly
 * processed.
 *
 * @param char1 the first Unicode character
 * @param char2 the second Unicode character
 * @param lang  language contexts to make better judgements
 * @return      one of \c LINEBREAK_MUSTBREAK, \c LINEBREAK_ALLOWBREAK,
 *				\c LINEBREAK_NOBREAK, or \c LINEBREAK_INSIDEACHAR
 */
int is_breakable(
		utf32_t char1,
		utf32_t char2,
		const char* lang)
{
	utf32_t s[2];
	char brks[2];
	s[0] = char1;
	s[1] = char2;
	set_linebreaks_utf32(s, 2, lang, brks);
	return brks[0];
}
