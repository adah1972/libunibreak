#!/usr/bin/env python3

import sys
import unicode_data_property


def output_east_asian_width_data(east_asian_width_properties, skip=''):
    print('static const struct EastAsianWidthProperties eaw_prop[] = {')
    for start, (end, prop) in east_asian_width_properties.items():
        if prop == skip:
            continue
        print(f"    {{0x{start:04X}, 0x{end:04X}, EAW_{prop}}},")
    print('};')


def lookup_east_asian_width(cp, east_asian_width_properties):
    for start, (end, prop) in east_asian_width_properties.items():
        if cp >= start and cp <= end:
            return prop


def output_is_op_east_asian_function(line_break_properties,
                                     east_asian_width_properties):
    print('bool ub_is_op_east_asian(utf32_t ch) {')
    print('    return false', end='')
    last_east_asian_op = None
    for start, (end, prop) in line_break_properties.items():
        if prop == 'OP' or prop == 'CP':
            for ch in range(start, end + 1):
                w = lookup_east_asian_width(ch, east_asian_width_properties)
                if w in ('F', 'W', 'H'):
                    # As of Unicode 15.1, there is no east asian CP. If this
                    # doesn't hold in a future version, we need to adjust code.
                    assert prop == 'OP'

                    if not last_east_asian_op:
                        print(f'\n        || (ch >= 0x{ch:04X}', end='')
                        last_east_asian_op = ch
                else:
                    if last_east_asian_op:
                        print(f' && ch <= 0x{ch:04X})', end='')
                        last_east_asian_op = None
    if last_east_asian_op:
        print(')', end='')
    print(';')
    print('}')


def main():
    input_file_path = sys.argv[1] if sys.argv[1:] else \
        'EastAsianWidth.txt'
    line_break_file = sys.argv[2] if len(sys.argv) > 2 else 'LineBreak.txt'
    leading_comment, east_asian_width_properties = \
        unicode_data_property.load_data(input_file_path)
    leading_comment_2, line_break_properties = \
        unicode_data_property.load_data(line_break_file)

    print('/* The content of this file is generated from:')
    print(leading_comment, end='')
    print(leading_comment_2, end='')
    print('*/')
    print()
    print('#include "eastasianwidthdef.h"')
    print()
    output_east_asian_width_data(east_asian_width_properties, skip='N')
    print()
    output_is_op_east_asian_function(line_break_properties,
                                     east_asian_width_properties)


if __name__ == '__main__':
    main()
