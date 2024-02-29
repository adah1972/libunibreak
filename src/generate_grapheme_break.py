#!/usr/bin/env python3

import sys

# Leading comment of the input file
leading_comment = ""


def parse_data_line(line):
    # Skip comments and empty lines
    if line.startswith('#') or not line.strip():
        return None

    # Parse the valid line to extract the range and the property
    range_part, prop = line.split(';')
    prop = prop.split('#')[0].strip()  # Clean up the property string

    # Handle single code points and ranges
    if '..' in range_part:
        start_str, end_str = range_part.split('..')
        start = int(start_str, 16)
        end = int(end_str, 16)
    else:
        start = end = int(range_part.strip(), 16)

    return start, end, prop


def load_data(file_path):
    global leading_comment
    result = {}

    with open(file_path, 'r', encoding='utf-8') as input_file:
        leading_comment = input_file.readline()
        leading_comment += input_file.readline()
        last_start = -1
        last_end = -1
        last_prop = ''
        for line in input_file:
            parsed_data = parse_data_line(line)
            if parsed_data:
                start, end, prop = parsed_data
                if last_end + 1 == start and last_prop == prop:
                    result[last_start] = (end, prop)
                    last_end = end
                else:
                    result[start] = (end, prop)
                    last_start, last_end, last_prop = start, end, prop

    # Sort by the start points
    result = {
        k: v for k, v in
        sorted(result.items(), key=lambda item: item[0])
    }
    return result


def output_grapheme_break_data(grapheme_break_properties):
    print('/* The content of this file is generated from:')
    print(leading_comment, end='')
    print('*/')
    print('')
    print('#include "graphemebreakdef.h"')
    print('')
    print('static const struct GraphemeBreakProperties gb_prop_default[] = {')
    for start, (end, prop) in grapheme_break_properties.items():
        print(f"\t{{0x{start:04X}, 0x{end:04X}, GBP_{prop}}},")
    print('};')


def main():
    input_file_path = sys.argv[1] if sys.argv[1:] else \
        'GraphemeBreakProperty.txt'
    grapheme_break_properties = load_data(input_file_path)
    output_grapheme_break_data(grapheme_break_properties)


if __name__ == '__main__':
    main()
