/* vim: set expandtab tabstop=4 softtabstop=4 shiftwidth=4: */

/*
 * Line breaking in a Unicode sequence.  Designed to be used in a
 * generic text renderer.
 *
 * Copyright (C) 2008-2026 Wu Yongwei <wuyongwei at gmail dot com>
 * Copyright (C) 2013 Petr Filipsky <philodej at gmail dot com>
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
 * The main reference is Unicode Standard Annex 14 (UAX #14):
 *      <URL:http://www.unicode.org/reports/tr14/>
 *
 * When this library was designed, this annex was at Revision 19, for
 * Unicode 5.0.0:
 *      <URL:http://www.unicode.org/reports/tr14/tr14-19.html>
 *
 * This library has been updated according to Revision 55, for
 * Unicode 17.0.0:
 *      <URL:http://www.unicode.org/reports/tr14/tr14-55.html>
 *
 * The Unicode Terms of Use are available at
 *      <URL:http://www.unicode.org/copyright.html>
 */

/**
 * @file    linebreakdef.h
 *
 * Definitions of internal data structures, declarations of global
 * variables, and function prototypes for the line breaking algorithm.
 *
 * @author  Wu Yongwei
 * @author  Petr Filipsky
 */

#ifndef LINEBREAKDEF_H
#define LINEBREAKDEF_H

#include "unibreakdef.h"

/**
 * Line break classes.  This is a mapping of Table 1 of Unicode
 * Standard Annex 14.
 */
enum LineBreakClass
{
    /* This is used to signal an error condition. */
    LBP_Undefined,  /**< Undefined */

    /* The following break classes are treated in the pair table. */
    LBP_OP,         /**< Opening punctuation */
    LBP_CL,         /**< Closing punctuation */
    LBP_CP,         /**< Closing parenthesis */
    LBP_QU,         /**< Ambiguous quotation */
    LBP_GL,         /**< Glue */
    LBP_NS,         /**< Non-starters */
    LBP_EX,         /**< Exclamation/Interrogation */
    LBP_SY,         /**< Symbols allowing break after */
    LBP_IS,         /**< Infix separator */
    LBP_PR,         /**< Prefix */
    LBP_PO,         /**< Postfix */
    LBP_NU,         /**< Numeric */
    LBP_AL,         /**< Alphabetic */
    LBP_HL,         /**< Hebrew letter */
    LBP_ID,         /**< Ideographic */
    LBP_IN,         /**< Inseparable characters */
    LBP_HY,         /**< Hyphen */
    LBP_BA,         /**< Break after */
    LBP_BB,         /**< Break before */
    LBP_B2,         /**< Break on either side (but not pair) */
    LBP_ZW,         /**< Zero-width space */
    LBP_CM,         /**< Combining marks */
    LBP_WJ,         /**< Word joiner */
    LBP_H2,         /**< Hangul LV */
    LBP_H3,         /**< Hangul LVT */
    LBP_JL,         /**< Hangul L Jamo */
    LBP_JV,         /**< Hangul V Jamo */
    LBP_JT,         /**< Hangul T Jamo */
    LBP_RI,         /**< Regional indicator */
    LBP_EB,         /**< Emoji base */
    LBP_EM,         /**< Emoji modifier */
    LBP_ZWJ,        /**< Zero width joiner */

    /* The following break classes are treated in the pair table, but
     * they are not part of Table 2 of UAX #14-37. */
    LBP_AK,         /**< Aksara */
    LBP_AP,         /**< Aksara Pre-Base */
    LBP_AS,         /**< Aksara Start */
    LBP_VF,         /**< Virama Final */
    LBP_VI,         /**< Virama */
    LBP_HH,         /**< Unambiguous Hyphen */
    LBP_CB,         /**< Contingent break */

    /* The following break classes are not treated in the pair table */
    LBP_AI,         /**< Ambiguous (alphabetic or ideograph) */
    LBP_BK,         /**< Break (mandatory) */
    LBP_CJ,         /**< Conditional Japanese starter */
    LBP_CR,         /**< Carriage return */
    LBP_LF,         /**< Line feed */
    LBP_NL,         /**< Next line */
    LBP_SA,         /**< South-East Asian */
    LBP_SG,         /**< Surrogates */
    LBP_SP,         /**< Space */
    LBP_XX          /**< Unknown */
};

/**
 * LB25 state for numeric expression context tracking.
 * Used for Example 7 regex-based tailoring from UAX \#14-49.
 */
enum Lb25State
{
    LB25_NONE,       /**< Not in numeric expression */
    LB25_PREFIX,     /**< Seen PR or PO (prefix) */
    LB25_PREFIXOP,   /**< Saw (PR|PO) then OP or HY */
    LB25_NUM,        /**< Inside NU (NU|SY|IS)* */
    LB25_NUMCLOSE,   /**< Saw NU (NU|SY|IS)* (CL|CP) */
};

/**
 * Identifies which lookahead rule currently has a tentative break
 * pending resolution by the next character.
 */
enum PendingRule
{
    PENDING_NONE,       /**< No pending tentative break */
    PENDING_LB15B,      /**< LB15b: tentative break before a Pf&QU */
    PENDING_LB15C,      /**< LB15c: tentative break before IS after SP */
    PENDING_LB19A,      /**< LB19a: tentative break before a Pi&QU */
    PENDING_LB28A4,     /**< LB28a sub-rule 4: tentative break after aksara */
};

enum BreakOutputType
{
    LBOT_PER_CODE_UNIT,
    LBOT_PER_CODE_POINT
};

/**
 * Struct for entries of line break properties.  The array of the
 * entries \e must be sorted.
 */
struct LineBreakProperties
{
    utf32_t start;              /**< Start codepoint */
    utf32_t end;                /**< End codepoint, inclusive */
    enum LineBreakClass prop;   /**< The line breaking property */
};

/**
 * Struct for entries of auxiliary line breaking properties derived from
 * the General_Category property.  The array of the entries \e must be
 * sorted.
 */
struct LineBreakAuxRange
{
    utf32_t start;              /**< Start codepoint */
    utf32_t end;                /**< End codepoint, inclusive */
};

/**
 * Struct for association of language-specific line breaking properties
 * with language names.
 */
struct LineBreakPropertiesLang
{
    const char *lang;                      /**< Language name */
    size_t namelen;                        /**< Length of name to match */
    const struct LineBreakProperties *lbp; /**< Pointer to associated data */
};

/**
 * Context representing internal state of the line breaking algorithm.
 * This is useful to callers if incremental analysis is wanted.
 *
 * The fields are ordered so that the members accessed on every
 * character sit first, while the rarely used lookahead-fixup state
 * sits last.
 */
struct LineBreakContext
{
    /* --- Hot state (touched on every character) --- */
    const struct LineBreakProperties *lbpLang; /**< Pointer to
                                                    LineBreakProperties */
    size_t posLast;                 /**< Last position in input string */
    enum LineBreakClass lbcCur;     /**< Breaking class of current codepoint */
    enum LineBreakClass lbcNew;     /**< Breaking class of next codepoint */
    enum LineBreakClass lbcLast;    /**< Breaking class of last codepoint */
    enum PendingRule ePending;      /**< Pending lookahead rule */
    enum Lb25State eLb25;           /**< LB25 state for numeric expression */
    int cLb30aRI;                   /**< Count of RI characters (LB30a) */

    bool fLb8aZwj;                  /**< Flag for ZWJ (LB8a) */
    bool fLb21aHebrew;              /**< Flag for Hebrew letters (LB21a) */
    bool fLb20aWordInit;            /**< Previous char is word-initial hyphen */
    bool fLb28aPrevAksara;          /**< Previous char is aksara (LB28a) */
    bool fLb28aAkVi;                /**< (AK|◌|AS) VI seen (LB28a) */

    bool fLangCjk;                  /**< zh/ja/ko language */
    bool fLangStrict;               /**< -strict suffix */
    bool fPrevPotentialEmoji;       /**< Previous char is potential emoji */
    bool fQuPiInitial;              /**< Previous Pi QU is in initial context */
    bool fPrevQuPi;                 /**< Previous char is QU of class Pi */
    bool fPrevQuPf;                 /**< Previous char is QU of class Pf */
    bool fPrevEA;                   /**< Previous char is East Asian */
    bool fQuPrevEA;                 /**< Char before previous QU is East Asian */

    /* --- Lookahead-fixup state (rarely used) --- */
    size_t posPending;              /**< Position of tentative break */
    size_t posLb25Fixup;            /**< Position to fix for LB25 */
    bool fLb25Mark;                 /**< Flag for pending fixup */
    char cPendingOrigBrk;           /**< Break value to restore on revert */
    bool fPendingRevert;            /**< Flag for pending revert */
};

/* Declarations */
extern const struct LineBreakProperties lb_prop_supplementary[];
extern const unsigned int lb_prop_supplementary_len;
extern const char lb_prop_bmp[];
extern const struct LineBreakPropertiesLang lb_prop_lang_map[];

/* Function Prototype */
void lb_init_break_context(
        struct LineBreakContext *lbpCtx,
        utf32_t ch,
        const char *lang);
int lb_process_next_char(
        struct LineBreakContext *lbpCtx,
        utf32_t ch);
enum LineBreakClass lb_get_char_class(
        const struct LineBreakContext *lbpCtx,
        utf32_t ch);
size_t set_linebreaks(
        const void *s,
        size_t len,
        const char *lang,
        enum BreakOutputType outputType,
        char *brks,
        get_next_char_t get_next_char);

#endif /* LINEBREAKDEF_H */
