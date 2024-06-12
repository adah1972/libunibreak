/* The content of this file is generated from:
# EastAsianWidth-15.1.0.txt
# Date: 2023-07-28, 23:34:08 GMT
# LineBreak-15.1.0.txt
# Date: 2023-07-28, 13:19:22 GMT [KW]
*/

#include "eastasianwidthdef.h"

bool op_is_east_asian(utf32_t ch) {
    return false
        || (ch >= 0x2329 && ch <= 0x2768)
        || (ch >= 0x3008 && ch <= 0xFD3F)
        || (ch >= 0xFE17 && ch <= 0x13258);
}
