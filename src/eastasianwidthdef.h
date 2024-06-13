/*
 * Definitions of internal data types for Indic Conjunct Break.
 *
 * Copyright (C) 2024 Wu Yongwei <wuyongwei at gmail dot com>
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
 */

#ifndef EASTASIANWIDTHDEF_H
#define EASTASIANWIDTHDEF_H

#include "unibreakdef.h"

/**
 * Returns whether an OP (Open Punctuation) is East Asian.
 *
 * For the purpose of supporting LB30, being "east asian" here means the char
 * has East_Asian_Width Property F/W/H.
 *
 * @param ch  Unicode codepoint (must be an OP)
 * @return    it is east asian or not
*/
bool op_is_east_asian(utf32_t ch);

#endif /* EASTASIANWIDTHDEF_H */
