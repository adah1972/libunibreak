#!/usr/bin/env python3

import sys

# Leading comment of the input file
leading_comment = ""
# This dictionary will map each start point to its corresponding end point and
# East Asian width property
east_asian_width_properties = {}


def parse_east_asian_width_line(line):
    # Skip comments and empty lines
    if line.startswith('#') or not line.strip():
        return None

    # Parse the valid line to extract the range and the InCB property
    range_part, prop = line.split(';')
    prop = prop.split('#')[0].strip()  # Clean up the InCB property string

    # Handle single code points and ranges
    if '..' in range_part:
        start_str, end_str = range_part.split('..')
        start = int(start_str, 16)
        end = int(end_str, 16)
    else:
        start = end = int(range_part.strip(), 16)

    return start, end, prop


def load_east_asian_width_data(file_path):
    global leading_comment
    global east_asian_width_properties

    with open(file_path, 'r', encoding='utf-8') as input_file:
        leading_comment = input_file.readline()
        leading_comment += input_file.readline()
        for line in input_file:
            parsed_data = parse_east_asian_width_line(line)
            if parsed_data:
                start, end, prop = parsed_data
                east_asian_width_properties[start] = (end, prop)


def output_east_asian_width_data():
    print('/* The content of this file is generated from:')
    print(leading_comment, end='')
    print('*/')
    print('')
    print('#include "eastasianwidthdef.h"')
    print('')
    print('static const struct EastAsianWidthProperties eaw_prop[] = {')
    for start, (end, prop) in east_asian_width_properties.items():
        print(f"    {{0x{start:04X}, 0x{end:04X}, EAW_{prop}}},")
    print('};')


def main():
    input_file_path = sys.argv[1] if sys.argv[1:] else \
        'EastAsianWidth.txt'
    load_east_asian_width_data(input_file_path)
    output_east_asian_width_data()


if __name__ == '__main__':
    main()
