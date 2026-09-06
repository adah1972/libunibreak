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
 * @file    linebreak.c
 *
 * Implementation of the line breaking algorithm as described in Unicode
 * Standard Annex 14.
 *
 * @author  Wu Yongwei
 * @author  Petr Filipsky
 */

#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include "eastasianwidthdef.h"
#include "linebreak.h"
#include "linebreakdef.h"
#include "linebreakauxdata.c"

/**
 * Codepoint of U+25CC DOTTED CIRCLE, which is treated as an aksara
 * (like class AK) for the purpose of rule LB28a only.
 */
#define LB28A_DOTTED_CIRCLE 0x25CC

#ifndef UB_LB25_OPT_HACK
/* See the later `#if UB_LB25_OPT_HACK` for how this optimization
 * works.  It proves to work well on GCC and MSVC, but not Clang,
 * which optimizes quite well by itself. */
#ifndef __clang__
#define UB_LB25_OPT_HACK 1
#else
#define UB_LB25_OPT_HACK 0
#endif
#endif

/**
 * Special value used internally to indicate an undefined break result.
 */
#define LINEBREAK_UNDEFINED -1

/**
 * Special value used internally to mark an invalid position.
 */
#define INVALID_POS ((size_t)-1)

/**
 * Enumeration of break actions.  They are used in the break action
 * pair table #baTable.
 */
enum BreakAction
{
    DIR_BRK,        /**< Direct break opportunity */
    IND_BRK,        /**< Indirect break opportunity */
    CMI_BRK,        /**< Indirect break opportunity for combining marks */
    CMP_BRK,        /**< Prohibited break for combining marks */
    PRH_BRK         /**< Prohibited break */
};

/**
 * Break action pair table.  This encodes the pair-level rules as
 * described in Table 2 of Unicode Standard Annex 14, Revision 37, with
 * the following manual adjustments:
 *
 * - CB is manually added as per LB20
 * - ZWJ is manually adjusted after special processing as per LB8a
 * - CL, CP, NS, SY, IS, PR, PO, HY, BA, B2, and RI are manually
 *   adjusted as per LB22
 * - AK, AP, AS, VF, VI, and HH are manually added as per LB28a and LB21
 */
static enum BreakAction baTable[LBP_CB][LBP_CB] = {
    {   /* OP */
        PRH_BRK, PRH_BRK, PRH_BRK, PRH_BRK, PRH_BRK, PRH_BRK, PRH_BRK,
        PRH_BRK, PRH_BRK, PRH_BRK, PRH_BRK, PRH_BRK, PRH_BRK, PRH_BRK,
        PRH_BRK, PRH_BRK, PRH_BRK, PRH_BRK, PRH_BRK, PRH_BRK, PRH_BRK,
        CMP_BRK, PRH_BRK, PRH_BRK, PRH_BRK, PRH_BRK, PRH_BRK, PRH_BRK,
        PRH_BRK, PRH_BRK, PRH_BRK, PRH_BRK, PRH_BRK, PRH_BRK, PRH_BRK,
        PRH_BRK, PRH_BRK, PRH_BRK, PRH_BRK
    },
    {   /* CL */
        DIR_BRK, PRH_BRK, PRH_BRK, IND_BRK, IND_BRK, PRH_BRK, PRH_BRK,
        PRH_BRK, PRH_BRK, IND_BRK, IND_BRK, DIR_BRK, DIR_BRK, DIR_BRK,
        DIR_BRK, IND_BRK, IND_BRK, IND_BRK, DIR_BRK, DIR_BRK, PRH_BRK,
        CMI_BRK, PRH_BRK, DIR_BRK, DIR_BRK, DIR_BRK, DIR_BRK, DIR_BRK,
        DIR_BRK, DIR_BRK, DIR_BRK, IND_BRK, DIR_BRK, DIR_BRK, DIR_BRK,
        DIR_BRK, DIR_BRK, IND_BRK, DIR_BRK
    },
    {   /* CP */
        DIR_BRK, PRH_BRK, PRH_BRK, IND_BRK, IND_BRK, PRH_BRK, PRH_BRK,
        PRH_BRK, PRH_BRK, IND_BRK, IND_BRK, DIR_BRK, DIR_BRK, DIR_BRK,
        DIR_BRK, IND_BRK, IND_BRK, IND_BRK, DIR_BRK, DIR_BRK, PRH_BRK,
        CMI_BRK, PRH_BRK, DIR_BRK, DIR_BRK, DIR_BRK, DIR_BRK, DIR_BRK,
        DIR_BRK, DIR_BRK, DIR_BRK, IND_BRK, DIR_BRK, DIR_BRK, DIR_BRK,
        DIR_BRK, DIR_BRK, IND_BRK, DIR_BRK
    },
    {   /* QU */
        IND_BRK, PRH_BRK, PRH_BRK, IND_BRK, IND_BRK, IND_BRK, PRH_BRK,
        PRH_BRK, PRH_BRK, IND_BRK, IND_BRK, IND_BRK, IND_BRK, IND_BRK,
        IND_BRK, IND_BRK, IND_BRK, IND_BRK, IND_BRK, IND_BRK, PRH_BRK,
        CMI_BRK, PRH_BRK, IND_BRK, IND_BRK, IND_BRK, IND_BRK, IND_BRK,
        IND_BRK, IND_BRK, IND_BRK, IND_BRK, IND_BRK, IND_BRK, IND_BRK,
        IND_BRK, IND_BRK, IND_BRK, IND_BRK
    },
    {   /* GL */
        IND_BRK, PRH_BRK, PRH_BRK, IND_BRK, IND_BRK, IND_BRK, PRH_BRK,
        PRH_BRK, PRH_BRK, IND_BRK, IND_BRK, IND_BRK, IND_BRK, IND_BRK,
        IND_BRK, IND_BRK, IND_BRK, IND_BRK, IND_BRK, IND_BRK, PRH_BRK,
        CMI_BRK, PRH_BRK, IND_BRK, IND_BRK, IND_BRK, IND_BRK, IND_BRK,
        IND_BRK, IND_BRK, IND_BRK, IND_BRK, IND_BRK, IND_BRK, IND_BRK,
        IND_BRK, IND_BRK, IND_BRK, IND_BRK
    },
    {   /* NS */
        DIR_BRK, PRH_BRK, PRH_BRK, IND_BRK, IND_BRK, IND_BRK, PRH_BRK,
        PRH_BRK, PRH_BRK, DIR_BRK, DIR_BRK, DIR_BRK, DIR_BRK, DIR_BRK,
        DIR_BRK, IND_BRK, IND_BRK, IND_BRK, DIR_BRK, DIR_BRK, PRH_BRK,
        CMI_BRK, PRH_BRK, DIR_BRK, DIR_BRK, DIR_BRK, DIR_BRK, DIR_BRK,
        DIR_BRK, DIR_BRK, DIR_BRK, IND_BRK, DIR_BRK, DIR_BRK, DIR_BRK,
        DIR_BRK, DIR_BRK, IND_BRK, DIR_BRK
    },
    {   /* EX */
        DIR_BRK, PRH_BRK, PRH_BRK, IND_BRK, IND_BRK, IND_BRK, PRH_BRK,
        PRH_BRK, PRH_BRK, DIR_BRK, DIR_BRK, DIR_BRK, DIR_BRK, DIR_BRK,
        DIR_BRK, IND_BRK, IND_BRK, IND_BRK, DIR_BRK, DIR_BRK, PRH_BRK,
        CMI_BRK, PRH_BRK, DIR_BRK, DIR_BRK, DIR_BRK, DIR_BRK, DIR_BRK,
        DIR_BRK, DIR_BRK, DIR_BRK, IND_BRK, DIR_BRK, DIR_BRK, DIR_BRK,
        DIR_BRK, DIR_BRK, IND_BRK, DIR_BRK
    },
    {   /* SY */
        DIR_BRK, PRH_BRK, PRH_BRK, IND_BRK, IND_BRK, IND_BRK, PRH_BRK,
        PRH_BRK, PRH_BRK, DIR_BRK, DIR_BRK, IND_BRK, DIR_BRK, IND_BRK,
        DIR_BRK, IND_BRK, IND_BRK, IND_BRK, DIR_BRK, DIR_BRK, PRH_BRK,
        CMI_BRK, PRH_BRK, DIR_BRK, DIR_BRK, DIR_BRK, DIR_BRK, DIR_BRK,
        DIR_BRK, DIR_BRK, DIR_BRK, IND_BRK, DIR_BRK, DIR_BRK, DIR_BRK,
        DIR_BRK, DIR_BRK, IND_BRK, DIR_BRK
    },
    {   /* IS */
        DIR_BRK, PRH_BRK, PRH_BRK, IND_BRK, IND_BRK, IND_BRK, PRH_BRK,
        PRH_BRK, PRH_BRK, DIR_BRK, DIR_BRK, IND_BRK, IND_BRK, IND_BRK,
        DIR_BRK, IND_BRK, IND_BRK, IND_BRK, DIR_BRK, DIR_BRK, PRH_BRK,
        CMI_BRK, PRH_BRK, DIR_BRK, DIR_BRK, DIR_BRK, DIR_BRK, DIR_BRK,
        DIR_BRK, DIR_BRK, DIR_BRK, IND_BRK, DIR_BRK, DIR_BRK, DIR_BRK,
        DIR_BRK, DIR_BRK, IND_BRK, DIR_BRK
    },
    {   /* PR */
        IND_BRK, PRH_BRK, PRH_BRK, IND_BRK, IND_BRK, IND_BRK, PRH_BRK,
        PRH_BRK, PRH_BRK, DIR_BRK, DIR_BRK, IND_BRK, IND_BRK, IND_BRK,
        IND_BRK, IND_BRK, IND_BRK, IND_BRK, DIR_BRK, DIR_BRK, PRH_BRK,
        CMI_BRK, PRH_BRK, IND_BRK, IND_BRK, IND_BRK, IND_BRK, IND_BRK,
        DIR_BRK, IND_BRK, IND_BRK, IND_BRK, DIR_BRK, DIR_BRK, DIR_BRK,
        DIR_BRK, DIR_BRK, IND_BRK, DIR_BRK
    },
    {   /* PO */
        IND_BRK, PRH_BRK, PRH_BRK, IND_BRK, IND_BRK, IND_BRK, PRH_BRK,
        PRH_BRK, PRH_BRK, DIR_BRK, DIR_BRK, IND_BRK, IND_BRK, IND_BRK,
        DIR_BRK, IND_BRK, IND_BRK, IND_BRK, DIR_BRK, DIR_BRK, PRH_BRK,
        CMI_BRK, PRH_BRK, DIR_BRK, DIR_BRK, DIR_BRK, DIR_BRK, DIR_BRK,
        DIR_BRK, DIR_BRK, DIR_BRK, IND_BRK, DIR_BRK, DIR_BRK, DIR_BRK,
        DIR_BRK, DIR_BRK, IND_BRK, DIR_BRK
    },
    {   /* NU */
        DIR_BRK, PRH_BRK, PRH_BRK, IND_BRK, IND_BRK, IND_BRK, PRH_BRK,
        PRH_BRK, PRH_BRK, IND_BRK, IND_BRK, IND_BRK, IND_BRK, IND_BRK,
        DIR_BRK, IND_BRK, IND_BRK, IND_BRK, DIR_BRK, DIR_BRK, PRH_BRK,
        CMI_BRK, PRH_BRK, DIR_BRK, DIR_BRK, DIR_BRK, DIR_BRK, DIR_BRK,
        DIR_BRK, DIR_BRK, DIR_BRK, IND_BRK, DIR_BRK, DIR_BRK, DIR_BRK,
        DIR_BRK, DIR_BRK, IND_BRK, DIR_BRK
    },
    {   /* AL */
        DIR_BRK, PRH_BRK, PRH_BRK, IND_BRK, IND_BRK, IND_BRK, PRH_BRK,
        PRH_BRK, PRH_BRK, IND_BRK, IND_BRK, IND_BRK, IND_BRK, IND_BRK,
        DIR_BRK, IND_BRK, IND_BRK, IND_BRK, DIR_BRK, DIR_BRK, PRH_BRK,
        CMI_BRK, PRH_BRK, DIR_BRK, DIR_BRK, DIR_BRK, DIR_BRK, DIR_BRK,
        DIR_BRK, DIR_BRK, DIR_BRK, IND_BRK, DIR_BRK, DIR_BRK, DIR_BRK,
        DIR_BRK, DIR_BRK, IND_BRK, DIR_BRK
    },
    {   /* HL */
        DIR_BRK, PRH_BRK, PRH_BRK, IND_BRK, IND_BRK, IND_BRK, PRH_BRK,
        PRH_BRK, PRH_BRK, IND_BRK, IND_BRK, IND_BRK, IND_BRK, IND_BRK,
        DIR_BRK, IND_BRK, IND_BRK, IND_BRK, DIR_BRK, DIR_BRK, PRH_BRK,
        CMI_BRK, PRH_BRK, DIR_BRK, DIR_BRK, DIR_BRK, DIR_BRK, DIR_BRK,
        DIR_BRK, DIR_BRK, DIR_BRK, IND_BRK, DIR_BRK, DIR_BRK, DIR_BRK,
        DIR_BRK, DIR_BRK, IND_BRK, DIR_BRK
    },
    {   /* ID */
        DIR_BRK, PRH_BRK, PRH_BRK, IND_BRK, IND_BRK, IND_BRK, PRH_BRK,
        PRH_BRK, PRH_BRK, DIR_BRK, IND_BRK, DIR_BRK, DIR_BRK, DIR_BRK,
        DIR_BRK, IND_BRK, IND_BRK, IND_BRK, DIR_BRK, DIR_BRK, PRH_BRK,
        CMI_BRK, PRH_BRK, DIR_BRK, DIR_BRK, DIR_BRK, DIR_BRK, DIR_BRK,
        DIR_BRK, DIR_BRK, DIR_BRK, IND_BRK, DIR_BRK, DIR_BRK, DIR_BRK,
        DIR_BRK, DIR_BRK, IND_BRK, DIR_BRK
    },
    {   /* IN */
        DIR_BRK, PRH_BRK, PRH_BRK, IND_BRK, IND_BRK, IND_BRK, PRH_BRK,
        PRH_BRK, PRH_BRK, DIR_BRK, DIR_BRK, DIR_BRK, DIR_BRK, DIR_BRK,
        DIR_BRK, IND_BRK, IND_BRK, IND_BRK, DIR_BRK, DIR_BRK, PRH_BRK,
        CMI_BRK, PRH_BRK, DIR_BRK, DIR_BRK, DIR_BRK, DIR_BRK, DIR_BRK,
        DIR_BRK, DIR_BRK, DIR_BRK, IND_BRK, DIR_BRK, DIR_BRK, DIR_BRK,
        DIR_BRK, DIR_BRK, IND_BRK, DIR_BRK
    },
    {   /* HY */
        DIR_BRK, PRH_BRK, PRH_BRK, IND_BRK, DIR_BRK, IND_BRK, PRH_BRK,
        PRH_BRK, PRH_BRK, DIR_BRK, DIR_BRK, IND_BRK, DIR_BRK, DIR_BRK,
        DIR_BRK, IND_BRK, IND_BRK, IND_BRK, DIR_BRK, DIR_BRK, PRH_BRK,
        CMI_BRK, PRH_BRK, DIR_BRK, DIR_BRK, DIR_BRK, DIR_BRK, DIR_BRK,
        DIR_BRK, DIR_BRK, DIR_BRK, IND_BRK, DIR_BRK, DIR_BRK, DIR_BRK,
        DIR_BRK, DIR_BRK, IND_BRK, DIR_BRK
    },
    {   /* BA */
        DIR_BRK, PRH_BRK, PRH_BRK, IND_BRK, DIR_BRK, IND_BRK, PRH_BRK,
        PRH_BRK, PRH_BRK, DIR_BRK, DIR_BRK, DIR_BRK, DIR_BRK, DIR_BRK,
        DIR_BRK, IND_BRK, IND_BRK, IND_BRK, DIR_BRK, DIR_BRK, PRH_BRK,
        CMI_BRK, PRH_BRK, DIR_BRK, DIR_BRK, DIR_BRK, DIR_BRK, DIR_BRK,
        DIR_BRK, DIR_BRK, DIR_BRK, IND_BRK, DIR_BRK, DIR_BRK, DIR_BRK,
        DIR_BRK, DIR_BRK, IND_BRK, DIR_BRK
    },
    {   /* BB */
        IND_BRK, PRH_BRK, PRH_BRK, IND_BRK, IND_BRK, IND_BRK, PRH_BRK,
        PRH_BRK, PRH_BRK, IND_BRK, IND_BRK, IND_BRK, IND_BRK, IND_BRK,
        IND_BRK, IND_BRK, IND_BRK, IND_BRK, IND_BRK, IND_BRK, PRH_BRK,
        CMI_BRK, PRH_BRK, IND_BRK, IND_BRK, IND_BRK, IND_BRK, IND_BRK,
        IND_BRK, IND_BRK, IND_BRK, IND_BRK, IND_BRK, IND_BRK, IND_BRK,
        IND_BRK, IND_BRK, IND_BRK, DIR_BRK
    },
    {   /* B2 */
        DIR_BRK, PRH_BRK, PRH_BRK, IND_BRK, IND_BRK, IND_BRK, PRH_BRK,
        PRH_BRK, PRH_BRK, DIR_BRK, DIR_BRK, DIR_BRK, DIR_BRK, DIR_BRK,
        DIR_BRK, IND_BRK, IND_BRK, IND_BRK, DIR_BRK, PRH_BRK, PRH_BRK,
        CMI_BRK, PRH_BRK, DIR_BRK, DIR_BRK, DIR_BRK, DIR_BRK, DIR_BRK,
        DIR_BRK, DIR_BRK, DIR_BRK, IND_BRK, DIR_BRK, DIR_BRK, DIR_BRK,
        DIR_BRK, DIR_BRK, IND_BRK, DIR_BRK
    },
    {   /* ZW */
        DIR_BRK, DIR_BRK, DIR_BRK, DIR_BRK, DIR_BRK, DIR_BRK, DIR_BRK,
        DIR_BRK, DIR_BRK, DIR_BRK, DIR_BRK, DIR_BRK, DIR_BRK, DIR_BRK,
        DIR_BRK, DIR_BRK, DIR_BRK, DIR_BRK, DIR_BRK, DIR_BRK, PRH_BRK,
        DIR_BRK, DIR_BRK, DIR_BRK, DIR_BRK, DIR_BRK, DIR_BRK, DIR_BRK,
        DIR_BRK, DIR_BRK, DIR_BRK, DIR_BRK, DIR_BRK, DIR_BRK, DIR_BRK,
        DIR_BRK, DIR_BRK, DIR_BRK, DIR_BRK
    },
    {   /* CM */
        DIR_BRK, PRH_BRK, PRH_BRK, IND_BRK, IND_BRK, IND_BRK, PRH_BRK,
        PRH_BRK, PRH_BRK, IND_BRK, IND_BRK, IND_BRK, IND_BRK, IND_BRK,
        DIR_BRK, IND_BRK, IND_BRK, IND_BRK, DIR_BRK, DIR_BRK, PRH_BRK,
        CMI_BRK, PRH_BRK, DIR_BRK, DIR_BRK, DIR_BRK, DIR_BRK, DIR_BRK,
        DIR_BRK, DIR_BRK, DIR_BRK, IND_BRK, DIR_BRK, DIR_BRK, DIR_BRK,
        DIR_BRK, DIR_BRK, IND_BRK, DIR_BRK
    },
    {   /* WJ */
        IND_BRK, PRH_BRK, PRH_BRK, IND_BRK, IND_BRK, IND_BRK, PRH_BRK,
        PRH_BRK, PRH_BRK, IND_BRK, IND_BRK, IND_BRK, IND_BRK, IND_BRK,
        IND_BRK, IND_BRK, IND_BRK, IND_BRK, IND_BRK, IND_BRK, PRH_BRK,
        CMI_BRK, PRH_BRK, IND_BRK, IND_BRK, IND_BRK, IND_BRK, IND_BRK,
        IND_BRK, IND_BRK, IND_BRK, IND_BRK, IND_BRK, IND_BRK, IND_BRK,
        IND_BRK, IND_BRK, IND_BRK, IND_BRK
    },
    {   /* H2 */
        DIR_BRK, PRH_BRK, PRH_BRK, IND_BRK, IND_BRK, IND_BRK, PRH_BRK,
        PRH_BRK, PRH_BRK, DIR_BRK, IND_BRK, DIR_BRK, DIR_BRK, DIR_BRK,
        DIR_BRK, IND_BRK, IND_BRK, IND_BRK, DIR_BRK, DIR_BRK, PRH_BRK,
        CMI_BRK, PRH_BRK, DIR_BRK, DIR_BRK, DIR_BRK, IND_BRK, IND_BRK,
        DIR_BRK, DIR_BRK, DIR_BRK, IND_BRK, DIR_BRK, DIR_BRK, DIR_BRK,
        DIR_BRK, DIR_BRK, IND_BRK, DIR_BRK
    },
    {   /* H3 */
        DIR_BRK, PRH_BRK, PRH_BRK, IND_BRK, IND_BRK, IND_BRK, PRH_BRK,
        PRH_BRK, PRH_BRK, DIR_BRK, IND_BRK, DIR_BRK, DIR_BRK, DIR_BRK,
        DIR_BRK, IND_BRK, IND_BRK, IND_BRK, DIR_BRK, DIR_BRK, PRH_BRK,
        CMI_BRK, PRH_BRK, DIR_BRK, DIR_BRK, DIR_BRK, DIR_BRK, IND_BRK,
        DIR_BRK, DIR_BRK, DIR_BRK, IND_BRK, DIR_BRK, DIR_BRK, DIR_BRK,
        DIR_BRK, DIR_BRK, IND_BRK, DIR_BRK
    },
    {   /* JL */
        DIR_BRK, PRH_BRK, PRH_BRK, IND_BRK, IND_BRK, IND_BRK, PRH_BRK,
        PRH_BRK, PRH_BRK, DIR_BRK, IND_BRK, DIR_BRK, DIR_BRK, DIR_BRK,
        DIR_BRK, IND_BRK, IND_BRK, IND_BRK, DIR_BRK, DIR_BRK, PRH_BRK,
        CMI_BRK, PRH_BRK, IND_BRK, IND_BRK, IND_BRK, IND_BRK, DIR_BRK,
        DIR_BRK, DIR_BRK, DIR_BRK, IND_BRK, DIR_BRK, DIR_BRK, DIR_BRK,
        DIR_BRK, DIR_BRK, IND_BRK, DIR_BRK
    },
    {   /* JV */
        DIR_BRK, PRH_BRK, PRH_BRK, IND_BRK, IND_BRK, IND_BRK, PRH_BRK,
        PRH_BRK, PRH_BRK, DIR_BRK, IND_BRK, DIR_BRK, DIR_BRK, DIR_BRK,
        DIR_BRK, IND_BRK, IND_BRK, IND_BRK, DIR_BRK, DIR_BRK, PRH_BRK,
        CMI_BRK, PRH_BRK, DIR_BRK, DIR_BRK, DIR_BRK, IND_BRK, IND_BRK,
        DIR_BRK, DIR_BRK, DIR_BRK, IND_BRK, DIR_BRK, DIR_BRK, DIR_BRK,
        DIR_BRK, DIR_BRK, IND_BRK, DIR_BRK
    },
    {   /* JT */
        DIR_BRK, PRH_BRK, PRH_BRK, IND_BRK, IND_BRK, IND_BRK, PRH_BRK,
        PRH_BRK, PRH_BRK, DIR_BRK, IND_BRK, DIR_BRK, DIR_BRK, DIR_BRK,
        DIR_BRK, IND_BRK, IND_BRK, IND_BRK, DIR_BRK, DIR_BRK, PRH_BRK,
        CMI_BRK, PRH_BRK, DIR_BRK, DIR_BRK, DIR_BRK, DIR_BRK, IND_BRK,
        DIR_BRK, DIR_BRK, DIR_BRK, IND_BRK, DIR_BRK, DIR_BRK, DIR_BRK,
        DIR_BRK, DIR_BRK, IND_BRK, DIR_BRK
    },
    {   /* RI */
        DIR_BRK, PRH_BRK, PRH_BRK, IND_BRK, IND_BRK, IND_BRK, PRH_BRK,
        PRH_BRK, PRH_BRK, DIR_BRK, DIR_BRK, DIR_BRK, DIR_BRK, DIR_BRK,
        DIR_BRK, IND_BRK, IND_BRK, IND_BRK, DIR_BRK, DIR_BRK, PRH_BRK,
        CMI_BRK, PRH_BRK, DIR_BRK, DIR_BRK, DIR_BRK, DIR_BRK, DIR_BRK,
        IND_BRK, DIR_BRK, DIR_BRK, IND_BRK, DIR_BRK, DIR_BRK, DIR_BRK,
        DIR_BRK, DIR_BRK, IND_BRK, DIR_BRK
    },
    {   /* EB */
        DIR_BRK, PRH_BRK, PRH_BRK, IND_BRK, IND_BRK, IND_BRK, PRH_BRK,
        PRH_BRK, PRH_BRK, DIR_BRK, IND_BRK, DIR_BRK, DIR_BRK, DIR_BRK,
        DIR_BRK, IND_BRK, IND_BRK, IND_BRK, DIR_BRK, DIR_BRK, PRH_BRK,
        CMI_BRK, PRH_BRK, DIR_BRK, DIR_BRK, DIR_BRK, DIR_BRK, DIR_BRK,
        DIR_BRK, DIR_BRK, IND_BRK, IND_BRK, DIR_BRK, DIR_BRK, DIR_BRK,
        DIR_BRK, DIR_BRK, IND_BRK, DIR_BRK
    },
    {   /* EM */
        DIR_BRK, PRH_BRK, PRH_BRK, IND_BRK, IND_BRK, IND_BRK, PRH_BRK,
        PRH_BRK, PRH_BRK, DIR_BRK, IND_BRK, DIR_BRK, DIR_BRK, DIR_BRK,
        DIR_BRK, IND_BRK, IND_BRK, IND_BRK, DIR_BRK, DIR_BRK, PRH_BRK,
        CMI_BRK, PRH_BRK, DIR_BRK, DIR_BRK, DIR_BRK, DIR_BRK, DIR_BRK,
        DIR_BRK, DIR_BRK, DIR_BRK, IND_BRK, DIR_BRK, DIR_BRK, DIR_BRK,
        DIR_BRK, DIR_BRK, IND_BRK, DIR_BRK
    },
    {   /* ZWJ */
        DIR_BRK, PRH_BRK, PRH_BRK, IND_BRK, IND_BRK, IND_BRK, PRH_BRK,
        PRH_BRK, PRH_BRK, IND_BRK, IND_BRK, IND_BRK, IND_BRK, IND_BRK,
        DIR_BRK, IND_BRK, IND_BRK, IND_BRK, DIR_BRK, DIR_BRK, PRH_BRK,
        CMI_BRK, PRH_BRK, DIR_BRK, DIR_BRK, DIR_BRK, DIR_BRK, DIR_BRK,
        DIR_BRK, DIR_BRK, DIR_BRK, IND_BRK, DIR_BRK, DIR_BRK, DIR_BRK,
        DIR_BRK, DIR_BRK, IND_BRK, DIR_BRK
    },
    {   /* AK */
        DIR_BRK, PRH_BRK, PRH_BRK, IND_BRK, IND_BRK, IND_BRK, PRH_BRK,
        PRH_BRK, PRH_BRK, DIR_BRK, DIR_BRK, DIR_BRK, DIR_BRK, DIR_BRK,
        DIR_BRK, IND_BRK, IND_BRK, IND_BRK, DIR_BRK, DIR_BRK, PRH_BRK,
        CMI_BRK, PRH_BRK, DIR_BRK, DIR_BRK, DIR_BRK, DIR_BRK, DIR_BRK,
        DIR_BRK, DIR_BRK, DIR_BRK, IND_BRK, DIR_BRK, DIR_BRK, DIR_BRK,
        IND_BRK, IND_BRK, IND_BRK, DIR_BRK
    },
    {   /* AP */
        DIR_BRK, PRH_BRK, PRH_BRK, IND_BRK, IND_BRK, IND_BRK, PRH_BRK,
        PRH_BRK, PRH_BRK, DIR_BRK, DIR_BRK, DIR_BRK, DIR_BRK, DIR_BRK,
        DIR_BRK, IND_BRK, IND_BRK, IND_BRK, DIR_BRK, DIR_BRK, PRH_BRK,
        CMI_BRK, PRH_BRK, DIR_BRK, DIR_BRK, DIR_BRK, DIR_BRK, DIR_BRK,
        DIR_BRK, DIR_BRK, DIR_BRK, IND_BRK, IND_BRK, DIR_BRK, IND_BRK,
        DIR_BRK, DIR_BRK, IND_BRK, DIR_BRK
    },
    {   /* AS */
        DIR_BRK, PRH_BRK, PRH_BRK, IND_BRK, IND_BRK, IND_BRK, PRH_BRK,
        PRH_BRK, PRH_BRK, DIR_BRK, DIR_BRK, DIR_BRK, DIR_BRK, DIR_BRK,
        DIR_BRK, IND_BRK, IND_BRK, IND_BRK, DIR_BRK, DIR_BRK, PRH_BRK,
        CMI_BRK, PRH_BRK, DIR_BRK, DIR_BRK, DIR_BRK, DIR_BRK, DIR_BRK,
        DIR_BRK, DIR_BRK, DIR_BRK, IND_BRK, DIR_BRK, DIR_BRK, DIR_BRK,
        IND_BRK, IND_BRK, IND_BRK, DIR_BRK
    },
    {   /* VF */
        DIR_BRK, PRH_BRK, PRH_BRK, IND_BRK, IND_BRK, IND_BRK, PRH_BRK,
        PRH_BRK, PRH_BRK, DIR_BRK, DIR_BRK, DIR_BRK, DIR_BRK, DIR_BRK,
        DIR_BRK, IND_BRK, IND_BRK, IND_BRK, DIR_BRK, DIR_BRK, PRH_BRK,
        CMI_BRK, PRH_BRK, DIR_BRK, DIR_BRK, DIR_BRK, DIR_BRK, DIR_BRK,
        DIR_BRK, DIR_BRK, DIR_BRK, IND_BRK, DIR_BRK, DIR_BRK, DIR_BRK,
        DIR_BRK, DIR_BRK, IND_BRK, DIR_BRK
    },
    {   /* VI */
        DIR_BRK, PRH_BRK, PRH_BRK, IND_BRK, IND_BRK, IND_BRK, PRH_BRK,
        PRH_BRK, PRH_BRK, DIR_BRK, DIR_BRK, DIR_BRK, DIR_BRK, DIR_BRK,
        DIR_BRK, IND_BRK, IND_BRK, IND_BRK, DIR_BRK, DIR_BRK, PRH_BRK,
        CMI_BRK, PRH_BRK, DIR_BRK, DIR_BRK, DIR_BRK, DIR_BRK, DIR_BRK,
        DIR_BRK, DIR_BRK, DIR_BRK, IND_BRK, DIR_BRK, DIR_BRK, DIR_BRK,
        DIR_BRK, DIR_BRK, IND_BRK, DIR_BRK
    },
    {   /* HH */
        DIR_BRK, PRH_BRK, PRH_BRK, IND_BRK, DIR_BRK, IND_BRK, PRH_BRK,
        PRH_BRK, PRH_BRK, DIR_BRK, DIR_BRK, DIR_BRK, DIR_BRK, DIR_BRK,
        DIR_BRK, IND_BRK, IND_BRK, IND_BRK, DIR_BRK, DIR_BRK, PRH_BRK,
        CMI_BRK, PRH_BRK, DIR_BRK, DIR_BRK, DIR_BRK, DIR_BRK, DIR_BRK,
        DIR_BRK, DIR_BRK, DIR_BRK, IND_BRK, DIR_BRK, DIR_BRK, DIR_BRK,
        DIR_BRK, DIR_BRK, IND_BRK, DIR_BRK
    },
    {   /* CB */
        DIR_BRK, PRH_BRK, PRH_BRK, IND_BRK, IND_BRK, DIR_BRK, PRH_BRK,
        PRH_BRK, PRH_BRK, DIR_BRK, DIR_BRK, DIR_BRK, DIR_BRK, DIR_BRK,
        DIR_BRK, DIR_BRK, DIR_BRK, DIR_BRK, DIR_BRK, DIR_BRK, PRH_BRK,
        CMI_BRK, PRH_BRK, DIR_BRK, DIR_BRK, DIR_BRK, DIR_BRK, DIR_BRK,
        DIR_BRK, DIR_BRK, DIR_BRK, IND_BRK, DIR_BRK, DIR_BRK, DIR_BRK,
        DIR_BRK, DIR_BRK, DIR_BRK, DIR_BRK
    }
};

/**
 * Checks whether the \a str ends with \a suffix, which has length
 * \a suffix_len.
 *
 * @param str        string whose ending is to be checked
 * @param suffix     string to check
 * @param suffixLen  length of \a suffix
 * @return           non-zero if true; zero otherwise
 */
static inline bool ends_with(const char *str, const char *suffix,
                             unsigned suffixLen)
{
    size_t len;
    if (str == NULL)
    {
        return false;
    }
    len = strlen(str);
    if (len >= suffixLen &&
        memcmp(str + len - suffixLen, suffix, suffixLen) == 0)
    {
        return true;
    }
    else
    {
        return false;
    }
}

static inline bool is_lang_cjk(const char *lang)
{
    if (lang == NULL)
    {
        return false;
    }

    return (strncmp(lang, "zh", 2) == 0 ||
            strncmp(lang, "ja", 2) == 0 ||
            strncmp(lang, "ko", 2) == 0);
}

#define ENDS_WITH(str, suffix) ends_with((str), (suffix), sizeof(suffix) - 1)

/**
 * Tells whether a codepoint is a QU quotation mark of General_Category
 * Pi (initial punctuation).
 */
static inline bool is_pi_qu(utf32_t ch)
{
    return ub_bsearch(ch, lb_prop_pi_qu, ARRAY_LEN(lb_prop_pi_qu),
                      sizeof(struct LineBreakAuxRange)) != NULL;
}

/**
 * Tells whether a codepoint is a QU quotation mark of General_Category
 * Pf (final punctuation).
 */
static inline bool is_pf_qu(utf32_t ch)
{
    return ub_bsearch(ch, lb_prop_pf_qu, ARRAY_LEN(lb_prop_pf_qu),
                      sizeof(struct LineBreakAuxRange)) != NULL;
}

/**
 * Tells whether a codepoint is an SA character of General_Category Mn
 * or Mc (i.e. resolves to CM as per rule LB1).
 */
static inline bool is_sa_cm(utf32_t ch)
{
    return ub_bsearch(ch, lb_prop_sa_cm, ARRAY_LEN(lb_prop_sa_cm),
                      sizeof(struct LineBreakAuxRange)) != NULL;
}

/**
 * Tells whether a codepoint is an unassigned extended pictograph (a
 * "potential emoji", as per rule LB30b).
 */
static inline bool is_potential_emoji(utf32_t ch)
{
    /* Potential emojis are unassigned extended pictographs, all in the
     * Supplementary Symbols and Pictographs block (U+1F000..).  The
     * threshold is derived from lb_prop_potential_emoji and should be
     * re-checked when that table is regenerated. */
    if (ch < 0x1F000)
    {
        return false;
    }
    return ub_bsearch(ch, lb_prop_potential_emoji,
                      ARRAY_LEN(lb_prop_potential_emoji),
                      sizeof(struct LineBreakAuxRange)) != NULL;
}

/**
 * Tells whether a codepoint has East Asian Width Fullwidth, Wide, or
 * Halfwidth (i.e. is "East Asian" for rule LB19a).
 */
static inline bool is_east_asian(utf32_t ch)
{
    /* East Asian width F/W/H characters all start at U+1100 (Hangul
     * Jamo).  The threshold is derived from eaw_prop and should be
     * re-checked when that table is regenerated. */
    if (ch < 0x1100)
    {
        return false;
    }
    enum EastAsianWidthClass eaw = ub_get_char_eaw_class(ch);
    return eaw == EAW_F || eaw == EAW_W || eaw == EAW_H;
}

/**
 * Tells whether a character (identified by its resolved line break
 * class \a lbc and codepoint \a ch) acts as an aksara for rule LB28a.
 */
static inline bool lb28a_is_aksara(enum LineBreakClass lbc, utf32_t ch)
{
    return lbc == LBP_AK || lbc == LBP_AS || ch == LB28A_DOTTED_CIRCLE;
}

/**
 * Does nothing.  This is kept for binary compatibility.
 */
void init_linebreak(void)
{
}

/**
 * Gets the language-specific line breaking properties.
 *
 * @param lang  language of the text
 * @return      pointer to the language-specific line breaking
 *              properties array if found; \c NULL otherwise
 */
static const struct LineBreakProperties *get_lb_prop_lang(const char *lang)
{
    const struct LineBreakPropertiesLang *lbplIter;
    if (lang != NULL)
    {
        for (lbplIter = lb_prop_lang_map; lbplIter->lang != NULL; ++lbplIter)
        {
            if (strncmp(lang, lbplIter->lang, lbplIter->namelen) == 0)
            {
                return lbplIter->lbp;
            }
        }
    }
    return NULL;
}

/**
 * Gets the line breaking class of a character from a line breaking
 * properties array.
 *
 * @param ch   character to check
 * @param lbp  pointer to the line breaking properties array
 * @return     the line breaking class if found; \c LBP_XX otherwise
 */
static enum LineBreakClass get_char_lb_class(
        utf32_t ch,
        const struct LineBreakProperties *lbp)
{
    while (lbp->prop != LBP_Undefined && ch >= lbp->start)
    {
        if (ch <= lbp->end)
        {
            return lbp->prop;
        }
        ++lbp;
    }
    return LBP_XX;
}

/**
 * Gets the line breaking class of a character from the default line
 * breaking properties array.
 *
 * @param ch  character to check
 * @return    the line breaking class if found; \c LBP_XX otherwise
 */
static enum LineBreakClass get_char_lb_class_default(utf32_t ch)
{
    if (ch < 65536)
    {
        return lb_prop_bmp[ch];
    }

    const struct LineBreakProperties *result_ptr =
        ub_bsearch(ch, lb_prop_supplementary, lb_prop_supplementary_len - 1,
                   sizeof(struct LineBreakProperties));
    if (result_ptr)
    {
        return result_ptr->prop;
    }

    return LBP_XX;
}

/**
 * Gets the line breaking class of a character for a specific
 * language.  This function will check the language-specific data first,
 * and then the default data if there is no language-specific property
 * available for the character.
 *
 * @param ch       character to check
 * @param lbpLang  pointer to the language-specific line breaking
 *                 properties array
 * @return         the line breaking class if found; \c LBP_XX
 *                 otherwise
 */
static enum LineBreakClass get_char_lb_class_lang(
        utf32_t ch,
        const struct LineBreakProperties *lbpLang)
{
    enum LineBreakClass lbcResult;

    /* Find the language-specific line breaking class for a character */
    if (lbpLang)
    {
        lbcResult = get_char_lb_class(ch, lbpLang);
        if (lbcResult != LBP_XX)
        {
            return lbcResult;
        }
    }

    /* Find the generic language-specific line breaking class, if no
     * language context is provided, or language-specific data are not
     * available for the specific character in the specified language */
    return get_char_lb_class_default(ch);
}

/**
 * Resolves the line breaking class for certain ambiguous or complicated
 * characters.  They are treated in a simplistic way in this
 * implementation.
 *
 * @param lbc     line breaking class to resolve
 * @param cjk     whether the language is Chinese, Japanese, or Korean
 * @param strict  whether the language has the \c -strict suffix
 * @param ch      the codepoint being resolved (for SA -> CM resolution)
 * @return        the resolved line breaking class
 */
static enum LineBreakClass resolve_lb_class(
        enum LineBreakClass lbc,
        bool cjk,
        bool strict,
        utf32_t ch)
{
    switch (lbc)
    {
    case LBP_AI:
        return cjk ? LBP_ID : LBP_AL;
    case LBP_CJ:
        /* `Strict' and `normal' line breaking.  See
         * <URL:http://www.unicode.org/reports/tr14/#CJ>
         * for details. */
        return strict ? LBP_NS : LBP_ID;
    case LBP_SA:
        /* Rule LB1: SA with General_Category Mn/Mc acts as CM. */
        return is_sa_cm(ch) ? LBP_CM : LBP_AL;
    case LBP_SG:
    case LBP_XX:
        return LBP_AL;
    default:
        return lbc;
    }
}

/**
 * Treats specially for the first character in a line.
 *
 * @param[in,out] lbpCtx  pointer to the line breaking context
 * @pre                   \a lbpCtx->lbcCur has a valid line break class
 * @post                  \a lbpCtx->lbcCur has the updated line break class
 */
static void treat_first_char(
        struct LineBreakContext *lbpCtx)
{
    lbpCtx->lbcNew = lbpCtx->lbcCur;
    switch (lbpCtx->lbcCur)
    {
    case LBP_LF:
    case LBP_NL:
        lbpCtx->lbcCur = LBP_BK;        /* Rule LB5 */
        break;
    case LBP_SP:
        lbpCtx->lbcCur = LBP_WJ;        /* Leading space treated as WJ */
        lbpCtx->lbcNew = LBP_SP;
        break;
    default:
        break;
    }
}

/**
 * Tries telling the line break opportunity by simple rules.
 *
 * @param[in,out] lbpCtx  pointer to the line breaking context
 * @pre                   \a lbpCtx->lbcCur has the current line break
 *                        class; and \a lbpCtx->lbcNew has the line
 *                        break class for the next character
 * @post                  \a lbpCtx->lbcCur has the updated line break
 *                        class
 * @return                break result, one of #LINEBREAK_MUSTBREAK,
 *                        #LINEBREAK_ALLOWBREAK, and #LINEBREAK_NOBREAK
 *                        if identified; or #LINEBREAK_UNDEFINED if
 *                        table lookup is needed
 */
static int get_lb_result_simple(
        struct LineBreakContext *lbpCtx)
{
    if (lbpCtx->lbcCur == LBP_BK ||
        (lbpCtx->lbcCur == LBP_CR && lbpCtx->lbcNew != LBP_LF))
    {
        return LINEBREAK_MUSTBREAK;     /* Rules LB4 and LB5 */
    }

    switch (lbpCtx->lbcNew)
    {
    case LBP_SP:
        return LINEBREAK_NOBREAK;       /* Rule LB7; no change to lbcCur */
    case LBP_BK:
    case LBP_LF:
    case LBP_NL:
        lbpCtx->lbcCur = LBP_BK;        /* Mandatory break after */
        return LINEBREAK_NOBREAK;       /* Rule LB6 */
    case LBP_CR:
        lbpCtx->lbcCur = LBP_CR;
        return LINEBREAK_NOBREAK;       /* Rule LB6 */
    default:
        return LINEBREAK_UNDEFINED;     /* Table lookup is needed */
    }
}

/**
 * Computes the next LB25 state from the current state and the class of
 * the character just read.  This is a pure function; the break value and
 * the fixup position are handled separately in #get_lb_result_decision.
 *
 * @param state   current LB25 state
 * @param lbcNew  line break class of the character just read
 * @return        the new LB25 state
 */
static enum Lb25State lb25_transition(
        enum Lb25State state, enum LineBreakClass lbcNew)
{
    switch (state)
    {
    case LB25_PREFIX:
        if (lbcNew == LBP_OP || lbcNew == LBP_HY)
        {
            return LB25_PREFIXOP;
        }
        break;
    case LB25_PREFIXOP:
        if (lbcNew == LBP_NU)
        {
            return LB25_NUM;
        }
        goto prefix_check;
    case LB25_NUM:
        if (lbcNew == LBP_NU || lbcNew == LBP_SY || lbcNew == LBP_IS)
        {
            return LB25_NUM;
        }
        if (lbcNew == LBP_CL || lbcNew == LBP_CP)
        {
            return LB25_NUMCLOSE;
        }
        /* FALLTHROUGH */
    case LB25_NUMCLOSE:
        if (lbcNew == LBP_PO || lbcNew == LBP_PR)
        {
            return LB25_PREFIX;
        }
        break;
    default:
        break;
    }

    if (lbcNew == LBP_NU)
    {
        return LB25_NUM;
    }

prefix_check:
    if (lbcNew == LBP_PR || lbcNew == LBP_PO)
    {
        return LB25_PREFIX;
    }

    return LB25_NONE;
}

/**
 * Tells whether a resolved line break class is "word-like" for the
 * purpose of rule LB15b: such a class is not in the follow set
 * (SP | GL | WJ | CL | QU | CP | EX | IS | SY | BK | CR | LF | NL | ZW
 * | eot), so a break before a preceding Pf&QU is allowed.
 */
static bool is_lb15b_word_like(enum LineBreakClass lbc)
{
    switch (lbc)
    {
    case LBP_SP:
    case LBP_GL:
    case LBP_WJ:
    case LBP_CL:
    case LBP_QU:
    case LBP_CP:
    case LBP_EX:
    case LBP_IS:
    case LBP_SY:
    case LBP_BK:
    case LBP_CR:
    case LBP_LF:
    case LBP_NL:
    case LBP_ZW:
        return false;
    default:
        return true;
    }
}

/**
 * Resolves the pending one-character lookahead fixup, based on the class
 * of the character just read.  This must run for every character,
 * including spaces and hard breaks, so it is invoked from
 * #lb_process_next_char rather than from #get_lb_result_lookup.
 *
 * Only one lookahead rule can be pending at a time, because their
 * trigger conditions are mutually exclusive.  When the lookahead fails,
 * #fPendingRevert is set so that #set_linebreaks restores the original
 * break value recorded when the tentative break was made.
 *
 * @param[in,out] lbpCtx  pointer to the line breaking context
 * @param[in]     lbcNew  the (resolved) class of the character just read
 * @param[in]     ch      the codepoint just read
 */
static void resolve_pending_break(
        struct LineBreakContext *lbpCtx,
        enum LineBreakClass lbcNew,
        utf32_t ch)
{
    switch (lbpCtx->ePending)
    {
    case PENDING_LB15B:
        /* Revert the tentative NOBREAK before a Pf&QU when the following
         * char is word-like. */
        if (is_lb15b_word_like(lbcNew))
        {
            lbpCtx->fPendingRevert = true;
        }
        break;
    case PENDING_LB15C:
        /* Confirm the tentative ALLOWBREAK before IS only when a NU
         * follows. */
        if (lbcNew != LBP_NU)
        {
            lbpCtx->fPendingRevert = true;
        }
        break;
    case PENDING_LB19A:
        /* Revert the tentative ALLOWBREAK before a Pi&QU when the
         * following char is not East Asian. */
        if (!is_east_asian(ch))
        {
            lbpCtx->fPendingRevert = true;
        }
        break;
    case PENDING_LB28A4:
        /* Revert the tentative NOBREAK between aksaras when a VF does
         * not follow. */
        if (lbcNew != LBP_VF)
        {
            lbpCtx->fPendingRevert = true;
        }
        break;
    default:
        break;
    }
    lbpCtx->ePending = PENDING_NONE;
}

/**
 * Determines the line break opportunity after the pair table lookup.
 * The rules are applied in ascending UAX #14 order, and the first match
 * wins (implemented with early returns).  The lookahead rules (LB15b,
 * LB15c, LB19a, and LB28a sub-rule 4) are applied last because they must
 * record the break value produced by the lower-precedence rules so that
 * it can be restored if the lookahead fails.
 *
 * @param[in,out] lbpCtx   pointer to the line breaking context
 * @param[in]     ch       the codepoint being resolved
 * @param[in]     isCurEA  whether \a ch has East Asian Width F/W/H
 * @param[in]     brk      the break value from the pair table lookup
 * @return                 break result, one of #LINEBREAK_MUSTBREAK,
 *                         #LINEBREAK_ALLOWBREAK, and #LINEBREAK_NOBREAK
 */
static int get_lb_result_decision(
        struct LineBreakContext *lbpCtx, utf32_t ch, bool isCurEA, int brk)
{
    bool curAksara = lb28a_is_aksara(lbpCtx->lbcNew, ch);
    bool curQuPi = (lbpCtx->lbcNew == LBP_QU && is_pi_qu(ch));
    bool curPfQu = (lbpCtx->lbcNew == LBP_QU && is_pf_qu(ch));

    /* Non-UAX tailoring for programmers: no break between "++", or
     * between '-' and '`'. */
    if ((lbpCtx->lbcLast == LBP_PR && ch == '+') ||
        (lbpCtx->lbcLast == LBP_HY && ch == '`'))
    {
        brk = LINEBREAK_NOBREAK;
    }

    /* Rule LB8a: ZWJ × */
    if (lbpCtx->fLb8aZwj)
    {
        return LINEBREAK_NOBREAK;
    }

    /* Rule LB15a: no break after an initial Pi&QU, even after spaces */
    if (lbpCtx->fQuPiInitial)
    {
        return LINEBREAK_NOBREAK;
    }

    /* Rule LB19a: break after a Pf&QU surrounded by East Asian
     * characters, unless a stronger rule prohibits a break before the
     * following char. */
    if (lbpCtx->fPrevQuPf && lbpCtx->fQuPrevEA && isCurEA &&
        lbpCtx->lbcNew != LBP_CL && lbpCtx->lbcNew != LBP_CP &&
        lbpCtx->lbcNew != LBP_EX && lbpCtx->lbcNew != LBP_SY &&
        lbpCtx->lbcNew != LBP_IS && lbpCtx->lbcNew != LBP_ZW &&
        lbpCtx->lbcNew != LBP_WJ && lbpCtx->lbcNew != LBP_CM)
    {
        return LINEBREAK_ALLOWBREAK;
    }

    /* Rule LB20a: do not break after a word-initial hyphen */
    if (lbpCtx->fLb20aWordInit && lbpCtx->lbcLast != LBP_SP &&
        (lbpCtx->lbcNew == LBP_AL || lbpCtx->lbcNew == LBP_HL))
    {
        return LINEBREAK_NOBREAK;
    }

    /* Rule LB21a: HL (HY | HH) × [^HL] */
    if (lbpCtx->fLb21aHebrew &&
        (lbpCtx->lbcCur == LBP_HY || lbpCtx->lbcCur == LBP_HH) &&
        lbpCtx->lbcNew != LBP_HL)
    {
        return LINEBREAK_NOBREAK;
    }

    /* Rule LB25 */
    if (lbpCtx->posLast != INVALID_POS)
    {
#if UB_LB25_OPT_HACK
        /* This hack reduces conditional jumps and works well with the
         * optimizers of GCC and MSVC. */
        static const uint16_t allow[LBP_PO + 1] = {
            [LBP_CL] = (1 << LBP_PR) | (1 << LBP_PO),
            [LBP_CP] = (1 << LBP_PR) | (1 << LBP_PO),
            [LBP_SY] = (1 << LBP_NU),
            [LBP_PR] = (1 << LBP_OP),
            [LBP_PO] = (1 << LBP_OP),
        };
        if (lbpCtx->lbcCur <= LBP_PO && lbpCtx->lbcNew <= LBP_NU &&
            (allow[lbpCtx->lbcCur] >> lbpCtx->lbcNew) & 1)
#else
        /* The Clang optimizer works well with the following condition.
         * Extra hacks harm the performance. */
        if ((lbpCtx->lbcCur == LBP_CL &&
             (lbpCtx->lbcNew == LBP_PO || lbpCtx->lbcNew == LBP_PR)) ||
            (lbpCtx->lbcCur == LBP_CP &&
             (lbpCtx->lbcNew == LBP_PO || lbpCtx->lbcNew == LBP_PR)) ||
            (lbpCtx->lbcCur == LBP_PO && lbpCtx->lbcNew == LBP_OP) ||
            (lbpCtx->lbcCur == LBP_PR && lbpCtx->lbcNew == LBP_OP) ||
            (lbpCtx->lbcCur == LBP_SY && lbpCtx->lbcNew == LBP_NU))
#endif
        {
            /* Allow break for the above cases, but later fixes may
             * change it again. */
            brk = LINEBREAK_ALLOWBREAK;
        }

        /* State-machine effects based on the current LB25 state. */
        switch (lbpCtx->eLb25)
        {
        case LB25_PREFIX:
            if (lbpCtx->lbcNew == LBP_OP || lbpCtx->lbcNew == LBP_HY)
            {
                lbpCtx->posLb25Fixup = lbpCtx->posLast;
            }
            break;
        case LB25_PREFIXOP:
            if (lbpCtx->lbcNew == LBP_NU)
            {
                lbpCtx->fLb25Mark = true;
            }
            else
            {
                lbpCtx->posLb25Fixup = INVALID_POS;
            }
            break;
        case LB25_NUM:
            if (lbpCtx->lbcNew == LBP_NU || lbpCtx->lbcNew == LBP_SY ||
                lbpCtx->lbcNew == LBP_IS)
            {
                brk = LINEBREAK_NOBREAK;
                break;
            }
            /* FALLTHROUGH */
        case LB25_NUMCLOSE:
            if (lbpCtx->lbcNew == LBP_PO || lbpCtx->lbcNew == LBP_PR)
            {
                brk = LINEBREAK_NOBREAK;
            }
            break;
        default:
            break;
        }
    }

    /* Rule LB28a sub-rules 1-3: do not break inside Brahmic
     * orthographic syllables */
    if (lbpCtx->lbcLast != LBP_SP)
    {
        /* Sub-rule 1: AP x (AK|DOTTED|AS) */
        if (lbpCtx->lbcCur == LBP_AP && curAksara)
        {
            return LINEBREAK_NOBREAK;
        }
        /* Sub-rule 2: (AK|DOTTED|AS) x (VF|VI) */
        if (lbpCtx->fLb28aPrevAksara &&
            (lbpCtx->lbcNew == LBP_VF || lbpCtx->lbcNew == LBP_VI))
        {
            return LINEBREAK_NOBREAK;
        }
        /* Sub-rule 3: (AK|DOTTED|AS) VI x (AK|DOTTED) */
        if (lbpCtx->fLb28aAkVi && curAksara)
        {
            return LINEBREAK_NOBREAK;
        }
    }

    /* Rule LB30 */
    if (/* (AL | HL | NU) × [OP-[\p{ea=F}\p{ea=W}\p{ea=H}]] */
        ((lbpCtx->lbcLast == LBP_AL || lbpCtx->lbcLast == LBP_HL ||
          lbpCtx->lbcLast == LBP_NU) &&
         (lbpCtx->lbcNew == LBP_OP && !ub_is_op_east_asian(ch))) ||
        /* [CP-[\p{ea=F}\p{ea=W}\p{ea=H}]] × (AL | HL | NU)
           note as of Unicode 15.1, there is no east asian CP
        */
        (lbpCtx->lbcLast == LBP_CP &&
         (lbpCtx->lbcNew == LBP_AL || lbpCtx->lbcNew == LBP_HL ||
          lbpCtx->lbcNew == LBP_NU)))
    {
        return LINEBREAK_NOBREAK;
    }

    /* Rule LB30a: break between pairs of regional indicators */
    if (lbpCtx->lbcCur == LBP_RI && lbpCtx->cLb30aRI == 1 &&
        lbpCtx->lbcNew == LBP_RI)
    {
        return LINEBREAK_ALLOWBREAK;
    }

    /* Rule LB30b: do not break between a potential emoji and EM */
    if (lbpCtx->fPrevPotentialEmoji && lbpCtx->lbcNew == LBP_EM)
    {
        return LINEBREAK_NOBREAK;
    }

    /* Rule LB15b: no break before a final Pf&QU, even after spaces.
     * Tentatively suppress the break; it is reverted when the following
     * char is word-like.  Rule LB8 (ZW) forces a break and takes
     * precedence, so skip that case. */
    if (curPfQu && lbpCtx->lbcCur != LBP_ZW)
    {
        lbpCtx->ePending = PENDING_LB15B;
        lbpCtx->posPending = lbpCtx->posLast;
        lbpCtx->cPendingOrigBrk = (char)brk;
        return LINEBREAK_NOBREAK;
    }

    /* Rule LB15c: break before a decimal mark that follows a space.
     * Tentatively break; it is confirmed only when a NU follows.  Rule
     * LB8 (ZW) already forces a break and takes precedence. */
    if (lbpCtx->lbcNew == LBP_IS && lbpCtx->lbcLast == LBP_SP &&
        lbpCtx->lbcCur != LBP_ZW)
    {
        lbpCtx->ePending = PENDING_LB15C;
        lbpCtx->posPending = lbpCtx->posLast;
        lbpCtx->cPendingOrigBrk = LINEBREAK_NOBREAK;
        return LINEBREAK_ALLOWBREAK;
    }

    /* Rule LB19a: break before a Pi&QU surrounded by East Asian
     * characters.  Tentatively break; it is reverted if the following
     * char is not East Asian. */
    if (curQuPi && lbpCtx->fPrevEA && lbpCtx->lbcLast != LBP_OP)
    {
        lbpCtx->ePending = PENDING_LB19A;
        lbpCtx->posPending = lbpCtx->posLast;
        lbpCtx->cPendingOrigBrk = LINEBREAK_NOBREAK;
        return LINEBREAK_ALLOWBREAK;
    }

    /* Rule LB28a sub-rule 4: (AK|DOTTED|AS) x (AK|DOTTED|AS) VF */
    if (lbpCtx->lbcLast != LBP_SP && lbpCtx->fLb28aPrevAksara && curAksara)
    {
        lbpCtx->ePending = PENDING_LB28A4;
        lbpCtx->posPending = lbpCtx->posLast;
        lbpCtx->cPendingOrigBrk = (char)brk;
        return LINEBREAK_NOBREAK;
    }

    return brk;
}

/**
 * Updates the per-character state flags, unconditionally.  This runs for
 * every character after #get_lb_result_decision, using the class values
 * of the character just read to prepare the state for the next one.
 *
 * @param[in,out] lbpCtx  pointer to the line breaking context
 * @param[in]     ch      the codepoint just read
 */
static void update_lb_state(
        struct LineBreakContext *lbpCtx, utf32_t ch)
{
    /* Rule LB21a: is the current char a Hebrew letter? */
    lbpCtx->fLb21aHebrew = (lbpCtx->lbcCur == LBP_HL);

    /* Rule LB25 state machine */
#if UB_LB25_OPT_HACK
    /* The else path is sufficient, but the additional condition may
     * avoid a function call and boost performance. */
    if (lbpCtx->eLb25 == LB25_NONE)
    {
        switch (lbpCtx->lbcNew)
        {
        case LBP_PR:
        case LBP_PO:
            lbpCtx->eLb25 = LB25_PREFIX;
            break;
        case LBP_NU:
            lbpCtx->eLb25 = LB25_NUM;
            break;
        default:
            break;
        }
    }
    else
#endif
    {
        lbpCtx->eLb25 = lb25_transition(lbpCtx->eLb25, lbpCtx->lbcNew);
    }

    /* Rule LB30a: track consecutive regional indicators */
    if (lbpCtx->lbcCur == LBP_RI)
    {
        ++lbpCtx->cLb30aRI;
        if (lbpCtx->cLb30aRI == 2 && lbpCtx->lbcNew == LBP_RI)
        {
            lbpCtx->cLb30aRI = 0;
        }
    }
    else
    {
        lbpCtx->cLb30aRI = 0;
    }

    /* Rule LB20a: is the current char a word-initial hyphen? */
    lbpCtx->fLb20aWordInit =
        (lbpCtx->lbcNew == LBP_HY || lbpCtx->lbcNew == LBP_HH) &&
        (lbpCtx->lbcLast == LBP_BK || lbpCtx->lbcLast == LBP_CR ||
         lbpCtx->lbcLast == LBP_LF || lbpCtx->lbcLast == LBP_NL ||
         lbpCtx->lbcLast == LBP_SP || lbpCtx->lbcLast == LBP_ZW ||
         lbpCtx->lbcLast == LBP_CB || lbpCtx->lbcLast == LBP_GL);

    /* Rule LB28a: track aksaras */
    lbpCtx->fLb28aAkVi =
        lbpCtx->fLb28aPrevAksara && lbpCtx->lbcNew == LBP_VI;
    lbpCtx->fLb28aPrevAksara = lb28a_is_aksara(lbpCtx->lbcNew, ch);

    /* Rule LB15a: is the current char an initial Pi&QU? */
    lbpCtx->fQuPiInitial =
        (lbpCtx->lbcNew == LBP_QU && is_pi_qu(ch)) &&
        (lbpCtx->lbcLast == LBP_BK || lbpCtx->lbcLast == LBP_CR ||
         lbpCtx->lbcLast == LBP_LF || lbpCtx->lbcLast == LBP_NL ||
         lbpCtx->lbcLast == LBP_OP || lbpCtx->lbcLast == LBP_QU ||
         lbpCtx->lbcLast == LBP_GL || lbpCtx->lbcLast == LBP_SP ||
         lbpCtx->lbcLast == LBP_ZW);
}

/**
 * Tells the line break opportunity by table lookup.
 *
 * @param[in,out] lbpCtx   pointer to the line breaking context
 * @param[in]     ch       the codepoint being resolved
 * @param[in]     isCurEA  whether \a ch has East Asian Width F/W/H
 * @pre                    \a lbpCtx->lbcCur has the current line break
 *                         class; \a lbpCtx->lbcLast has the line break
 *                         class for the last character; and \a
 *                         lbcCur->lbcNew has the line break class for
 *                         the next character
 * @post                   \a lbpCtx->lbcCur has the updated line break
 *                         class
 * @return                 break result, one of #LINEBREAK_MUSTBREAK,
 *                         #LINEBREAK_ALLOWBREAK, and #LINEBREAK_NOBREAK
 */
static int get_lb_result_lookup(
        struct LineBreakContext *lbpCtx, utf32_t ch, bool isCurEA)
{
    int brk = LINEBREAK_UNDEFINED;

    assert(lbpCtx->lbcCur <= LBP_CB);
    assert(lbpCtx->lbcNew <= LBP_CB);
    switch (baTable[lbpCtx->lbcCur - 1][lbpCtx->lbcNew - 1])
    {
    case DIR_BRK:
        brk = LINEBREAK_ALLOWBREAK;
        break;
    case IND_BRK:
        brk = (lbpCtx->lbcLast == LBP_SP)
            ? LINEBREAK_ALLOWBREAK
            : LINEBREAK_NOBREAK;
        break;
    case CMI_BRK:
        brk = LINEBREAK_ALLOWBREAK;
        if (lbpCtx->lbcLast != LBP_SP)
        {
            lbpCtx->eLb25 = LB25_NONE;
            return LINEBREAK_NOBREAK;   /* Do not update lbcCur or state */
        }
        break;
    case CMP_BRK:
        brk = LINEBREAK_NOBREAK;
        if (lbpCtx->lbcLast != LBP_SP)
        {
            lbpCtx->eLb25 = LB25_NONE;
            return LINEBREAK_NOBREAK;   /* Do not update lbcCur or state */
        }
        break;
    case PRH_BRK:
        brk = LINEBREAK_NOBREAK;
        break;
    }

    brk = get_lb_result_decision(lbpCtx, ch, isCurEA, brk);
    update_lb_state(lbpCtx, ch);
    lbpCtx->lbcCur = lbpCtx->lbcNew;
    return brk;
}

/**
 * Initializes line breaking context for a given language.
 *
 * @param[in,out] lbpCtx  pointer to the line breaking context
 * @param[in]     ch      the first character to process
 * @param[in]     lang    language of the input
 * @post                  the line breaking context is initialized
 */
void lb_init_break_context(
        struct LineBreakContext *lbpCtx,
        utf32_t ch,
        const char *lang)
{
    bool cjk = is_lang_cjk(lang);
    bool strict = ENDS_WITH(lang, "-strict");
    const struct LineBreakProperties *lbpLang = get_lb_prop_lang(lang);
    enum LineBreakClass rawLbc = get_char_lb_class_lang(ch, lbpLang);
    enum LineBreakClass lbcCur = resolve_lb_class(rawLbc, cjk, strict, ch);

    /* Members not listed below are zero-initialized (false, 0, etc.). */
    *lbpCtx = (struct LineBreakContext) {
        .lbpLang = lbpLang,
        .posLast = INVALID_POS,
        .lbcCur = lbcCur,
        .fLb8aZwj = (rawLbc == LBP_ZWJ),
        .fLb20aWordInit = (lbcCur == LBP_HY || lbcCur == LBP_HH),
        .fLb28aPrevAksara = lb28a_is_aksara(lbcCur, ch),
        .fLangCjk = cjk,
        .fLangStrict = strict,
        .fPrevPotentialEmoji = is_potential_emoji(ch),
        .fQuPiInitial = is_pi_qu(ch),
        .fPrevQuPi = is_pi_qu(ch),
        .fPrevQuPf = is_pf_qu(ch),
        .fPrevEA = is_east_asian(ch),
        .posPending = INVALID_POS,
        .posLb25Fixup = INVALID_POS,
        .cPendingOrigBrk = LINEBREAK_UNDEFINED,
    };
    treat_first_char(lbpCtx);
}

/**
 * Updates LineBreakingContext for the next codepoint and returns
 * the detected break.
 *
 * This function is deprecated, as it cannot support fixups, as
 * required by LB25 tailoring (and some more recent rules).  See the
 * implementation of #set_linebreaks for the fixup logic.
 *
 * @param[in,out] lbpCtx  pointer to the line breaking context
 * @param[in]     ch      Unicode codepoint
 * @return                break result, one of #LINEBREAK_MUSTBREAK,
 *                        #LINEBREAK_ALLOWBREAK, and #LINEBREAK_NOBREAK
 * @post                  the line breaking context is updated
 */
int lb_process_next_char(
        struct LineBreakContext *lbpCtx,
        utf32_t ch )
{
    int brk;

    /* Rule LB9 */
    if (!(lbpCtx->lbcNew == LBP_CM || lbpCtx->lbcNew == LBP_ZWJ) ||
        lbpCtx->lbcLast == LBP_BK || lbpCtx->lbcLast == LBP_CR ||
        lbpCtx->lbcLast == LBP_LF || lbpCtx->lbcLast == LBP_NL ||
        lbpCtx->lbcLast == LBP_SP || lbpCtx->lbcLast == LBP_ZW ||
        lbpCtx->lbcLast == LBP_Undefined)
    {
        lbpCtx->lbcLast = lbpCtx->lbcNew;
    }
    /* Rulle LB10 */
    if (lbpCtx->lbcLast == LBP_CM || lbpCtx->lbcLast == LBP_ZWJ)
    {
        lbpCtx->lbcLast = LBP_AL;
    }

    lbpCtx->lbcNew = get_char_lb_class_lang(ch, lbpCtx->lbpLang);
    lbpCtx->lbcNew = resolve_lb_class(lbpCtx->lbcNew, lbpCtx->fLangCjk,
                                      lbpCtx->fLangStrict, ch);
    bool isCurEA = is_east_asian(ch);
    resolve_pending_break(lbpCtx, lbpCtx->lbcNew, ch);
    brk = get_lb_result_simple(lbpCtx);
    switch (brk)
    {
    case LINEBREAK_MUSTBREAK:
        lbpCtx->lbcCur = lbpCtx->lbcNew;
        treat_first_char(lbpCtx);
        /* The char after a hard break acts like the start of text. */
        lbpCtx->fQuPiInitial =
            lbpCtx->lbcCur == LBP_QU && is_pi_qu(ch);
        lbpCtx->fLb20aWordInit =
            lbpCtx->lbcCur == LBP_HY || lbpCtx->lbcCur == LBP_HH;
        break;
    case LINEBREAK_UNDEFINED:
        brk = get_lb_result_lookup(lbpCtx, ch, isCurEA);
        break;
    default:
        lbpCtx->eLb25 = LB25_NONE;
        break;
    }

    /* Special processing due to rule LB8a */
    lbpCtx->fLb8aZwj = lbpCtx->lbcNew == LBP_ZWJ;

    /* Update the quotation/hyphen state, persisting through CM/ZWJ */
    if (lbpCtx->lbcNew != LBP_CM && lbpCtx->lbcNew != LBP_ZWJ)
    {
        lbpCtx->fPrevPotentialEmoji = is_potential_emoji(ch);
        lbpCtx->fPrevQuPi = lbpCtx->lbcNew == LBP_QU && is_pi_qu(ch);
        lbpCtx->fPrevQuPf = lbpCtx->lbcNew == LBP_QU && is_pf_qu(ch);
        if (lbpCtx->lbcNew == LBP_QU)
        {
            /* Record whether the char preceding this QU is East Asian. */
            lbpCtx->fQuPrevEA = lbpCtx->fPrevEA;
        }
        lbpCtx->fPrevEA = isCurEA;
    }

    return brk;
}

/**
 * Gets the line breaking class of a character for a line breaking
 * context.  This function will check the language-specific data first,
 * and then the default data if there is no language-specific property
 * available for the character.
 *
 * @param lbpCtx  pointer to the line breaking context
 * @param ch      character to check
 * @return        the line breaking class if found; \c LBP_XX otherwise
 */
enum LineBreakClass lb_get_char_class(
        const struct LineBreakContext *lbpCtx,
        utf32_t ch)
{
    return get_char_lb_class_lang(ch, lbpCtx->lbpLang);
}

/**
 * Sets the line breaking information for a generic input string.
 *
 * Currently, this implementation has customization for the following
 * ISO 639-1 language codes (for \a lang):
 *
 *  - de (German)
 *  - en (English)
 *  - es (Spanish)
 *  - fr (French)
 *  - ja (Japanese)
 *  - ko (Korean)
 *  - ru (Russian)
 *  - zh (Chinese)
 *
 * In addition, a suffix <code>"-strict"</code> may be added to indicate
 * strict (as versus normal) line-breaking behaviour.  See the <a
 * href="http://www.unicode.org/reports/tr14/#CJ">Conditional Japanese
 * Starter section of UAX #14</a> for more details.
 *
 * @param[in]  s             input string
 * @param[in]  len           length of the input
 * @param[in]  lang          language of the input
 * @param[in]  outputType    output per code-unit or per code-point
 * @param[out] brks          pointer to the output breaking data,
 *                           containing #LINEBREAK_MUSTBREAK,
 *                           #LINEBREAK_ALLOWBREAK, #LINEBREAK_NOBREAK,
 *                           or #LINEBREAK_INSIDEACHAR
 * @param[in] get_next_char  function to get the next UTF-32 character
 * @return       The number of entries in brks filled. This is equal to
 *               the number of code-points or code-units in the source
 *               string, depending on the outputType parameter.
 */
size_t set_linebreaks(
        const void *s,
        size_t len,
        const char *lang,
        enum BreakOutputType outputType,
        char *brks,
        get_next_char_t get_next_char)
{
    utf32_t ch;
    int lastBreak;
    struct LineBreakContext lbCtx;
    size_t posCur = 0;
    size_t posLast = 0;

    --posLast;  /* To be ++'d later */
    ch = get_next_char(s, len, &posCur);
    if (ch == EOS)
    {
        return 0;
    }
    lb_init_break_context(&lbCtx, ch, lang);

    /* Process a line till an explicit break or end of string */
    for (;;)
    {
        if (outputType == LBOT_PER_CODE_UNIT)
        {
            for (++posLast; posLast < posCur - 1; ++posLast)
            {
                brks[posLast] = LINEBREAK_INSIDEACHAR;
            }
            assert(posLast == posCur - 1);
        }
        else
        {
            posLast++;
        }
        ch = get_next_char(s, len, &posCur);
        if (ch == EOS)
        {
            break;
        }
        lbCtx.posLast = posLast;
        brks[posLast] = (char)lb_process_next_char(&lbCtx, ch);

        /* Fix-up due to LB25 */
        if (lbCtx.posLb25Fixup != INVALID_POS && lbCtx.fLb25Mark)
        {
            brks[lbCtx.posLb25Fixup] = LINEBREAK_NOBREAK;
            lbCtx.posLb25Fixup = INVALID_POS;
            lbCtx.fLb25Mark = false;
        }

        /* Fix-up (revert) due to a pending lookahead break */
        if (lbCtx.fPendingRevert)
        {
            brks[lbCtx.posPending] = lbCtx.cPendingOrigBrk;
            lbCtx.fPendingRevert = false;
            lbCtx.posPending = INVALID_POS;
        }
    }

    /* Revert any unresolved tentative break (LB28a sub-rule 4, LB15c, or
     * LB19a).  LB15b needs no revert at end of text, since its NOBREAK
     * result is correct when no word-like char follows. */
    switch (lbCtx.ePending)
    {
    case PENDING_LB15C:
    case PENDING_LB19A:
    case PENDING_LB28A4:
        brks[lbCtx.posPending] = lbCtx.cPendingOrigBrk;
        break;
    default:
        break;
    }
    lbCtx.ePending = PENDING_NONE;

    /* After the last character */
    lastBreak = get_lb_result_simple(&lbCtx);
    brks[posLast] = (lastBreak == LINEBREAK_MUSTBREAK)
                        ? LINEBREAK_MUSTBREAK
                        : LINEBREAK_INDETERMINATE;

    if (outputType == LBOT_PER_CODE_UNIT)
    {
        assert(posLast == posCur - 1 && posCur <= len);
        /* When the input contains incomplete sequences */
        while (posCur < len)
        {
            brks[posCur++] = LINEBREAK_INSIDEACHAR;
        }

        return posCur;
    }
    else
    {
        return posLast + 1;
    }
}

/**
 * Sets the line breaking information for a UTF-8 input string.
 *
 * @param[in]  s     input UTF-8 string
 * @param[in]  len   length of the input
 * @param[in]  lang  language of the input
 * @param[out] brks  pointer to the output breaking data, containing
 *                   #LINEBREAK_MUSTBREAK, #LINEBREAK_ALLOWBREAK,
 *                   #LINEBREAK_NOBREAK, or #LINEBREAK_INSIDEACHAR
 * @see #set_linebreaks for a note about \a lang.
 */
void set_linebreaks_utf8(
        const utf8_t *s,
        size_t len,
        const char *lang,
        char *brks)
{
    set_linebreaks(s, len, lang, LBOT_PER_CODE_UNIT, brks,
                   (get_next_char_t)ub_get_next_char_utf8);
}

/**
 * Sets the line breaking information for a UTF-8 input string.
 *
 * @param[in]  s     input UTF-8 string
 * @param[in]  len   length of the input
 * @param[in]  lang  language of the input
 * @param[out] brks  pointer to the output breaking data, containing
 *                   #LINEBREAK_MUSTBREAK, #LINEBREAK_ALLOWBREAK,
 *                   #LINEBREAK_NOBREAK
 * @return       The number of entries in brks filled. This is equal to
 *               the number of code-points in the source string.
 * @see #set_linebreaks for a note about \a lang.
 */
size_t set_linebreaks_utf8_per_code_point(
        const utf8_t *s,
        size_t len,
        const char *lang,
        char *brks)
{
    return set_linebreaks(s, len, lang, LBOT_PER_CODE_POINT, brks,
                          (get_next_char_t)ub_get_next_char_utf8);
}

/**
 * Sets the line breaking information for a UTF-16 input string.
 *
 * @param[in]  s     input UTF-16 string
 * @param[in]  len   length of the input
 * @param[in]  lang  language of the input
 * @param[out] brks  pointer to the output breaking data, containing
 *                   #LINEBREAK_MUSTBREAK, #LINEBREAK_ALLOWBREAK,
 *                   #LINEBREAK_NOBREAK, or #LINEBREAK_INSIDEACHAR
 * @see #set_linebreaks for a note about \a lang.
 */
void set_linebreaks_utf16(
        const utf16_t *s,
        size_t len,
        const char *lang,
        char *brks)
{
    set_linebreaks(s, len, lang, LBOT_PER_CODE_UNIT, brks,
                   (get_next_char_t)ub_get_next_char_utf16);
}

/**
 * Sets the line breaking information for a UTF-16 input string.
 *
 * @param[in]  s     input UTF-16 string
 * @param[in]  len   length of the input
 * @param[in]  lang  language of the input
 * @param[out] brks  pointer to the output breaking data, containing
 *                   #LINEBREAK_MUSTBREAK, #LINEBREAK_ALLOWBREAK,
 *                   #LINEBREAK_NOBREAK
 * @return       The number of entries in brks filled. This is equal to
 *               the number of code-points in the source string.
 * @see #set_linebreaks for a note about \a lang.
 */
size_t set_linebreaks_utf16_per_code_point(
        const utf16_t *s,
        size_t len,
        const char *lang,
        char *brks)
{
    return set_linebreaks(s, len, lang, LBOT_PER_CODE_POINT, brks,
                          (get_next_char_t)ub_get_next_char_utf16);
}

/**
 * Sets the line breaking information for a UTF-32 input string.
 *
 * @param[in]  s     input UTF-32 string
 * @param[in]  len   length of the input
 * @param[in]  lang  language of the input
 * @param[out] brks  pointer to the output breaking data, containing
 *                   #LINEBREAK_MUSTBREAK, #LINEBREAK_ALLOWBREAK,
 *                   #LINEBREAK_NOBREAK, or #LINEBREAK_INSIDEACHAR
 * @see #set_linebreaks for a note about \a lang.
 */
void set_linebreaks_utf32(
        const utf32_t *s,
        size_t len,
        const char *lang,
        char *brks)
{
    set_linebreaks(s, len, lang, LBOT_PER_CODE_UNIT, brks,
                   (get_next_char_t)ub_get_next_char_utf32);
}

/**
 * Tells whether a line break can occur between two Unicode characters.
 * This is a wrapper function to expose a simple interface.  Generally
 * speaking, it is better to use #set_linebreaks_utf32 instead, since
 * complicated cases involving combining marks, spaces, etc. cannot be
 * correctly processed.
 *
 * @param char1  the first Unicode character
 * @param char2  the second Unicode character
 * @param lang   language of the input
 * @return       one of #LINEBREAK_MUSTBREAK, #LINEBREAK_ALLOWBREAK,
 *               #LINEBREAK_NOBREAK, or #LINEBREAK_INSIDEACHAR
 */
int is_line_breakable(
        utf32_t char1,
        utf32_t char2,
        const char *lang)
{
    utf32_t s[2];
    char brks[2];
    s[0] = char1;
    s[1] = char2;
    set_linebreaks_utf32(s, 2, lang, brks);
    return brks[0];
}
