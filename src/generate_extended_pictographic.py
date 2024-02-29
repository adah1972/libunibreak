#!/usr/bin/env python3

import sys

# Leading comment of the input file
leading_comment = ""
# This dictionary will map each start point to its corresponding end point and
# emoji property
emoji_data = {}


def parse_emoji_data_line(line):
    # Skip comments and empty lines
    if line.startswith('#') or not line.strip():
        return None

    # Parse the valid line to extract the range and the emoji property
    range_part, prop = line.split(';')
    prop = prop.split('#')[0].strip()  # Clean up the emoji property string

    # Handle single code points and ranges
    if '..' in range_part:
        start_str, end_str = range_part.split('..')
        start = int(start_str, 16)
        end = int(end_str, 16)
    else:
        start = end = int(range_part.strip(), 16)

    return start, end, prop


def load_emoji_data(file_path):
    global leading_comment
    global emoji_data

    with open(file_path, 'r', encoding='utf-8') as input_file:
        leading_comment = input_file.readline()
        leading_comment += input_file.readline()
        last_start = -1
        last_prop = ''
        last_end = -1
        for line in input_file:
            parsed_data = parse_emoji_data_line(line)
            if parsed_data:
                start, end, prop = parsed_data
                if last_end + 1 == start and last_prop == prop:
                    emoji_data[last_start] = (end, prop)
                    last_end = end
                else:
                    emoji_data[start] = (end, prop)
                    last_start, last_end, last_prop = start, end, prop

    # Sort by the start points
    emoji_data = {
        k: v for k, v in
        sorted(emoji_data.items(), key=lambda item: item[0])
    }


def output_extended_pictographic_data():
    print('/* The content of this file is generated from:')
    print(leading_comment, end='')
    print('*/')
    print('')
    print('#include "emojidef.h"')
    print('')
    print('static const struct ExtendedPictograpic ep_prop[] = {')
    for start, (end, prop) in emoji_data.items():
        if prop == 'Extended_Pictographic':
            print(f"\t{{0x{start:04X}, 0x{end:04X}}},")
    print('};')


def main():
    input_file_path = sys.argv[1] if sys.argv[1:] else 'emoji-data.txt'
    load_emoji_data(input_file_path)
    output_extended_pictographic_data()


if __name__ == '__main__':
    main()
