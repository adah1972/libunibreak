#!/usr/bin/env python3

import re
import sys


def load_ranges(file_path, prop_filter=None):
    ranges = []
    with open(file_path, 'r', encoding='utf-8') as input_file:
        for line in input_file:
            if line.startswith('#') or not line.strip():
                continue
            m = re.match(r'\s*([0-9A-F]+)(?:\.\.([0-9A-F]+))?\s*;\s*'
                         r'([A-Za-z_]+)', line)
            if not m:
                continue
            start = int(m.group(1), 16)
            end = int(m.group(2), 16) if m.group(2) else start
            prop = m.group(3)
            if prop_filter is None or prop in prop_filter:
                ranges.append((start, end, prop))
    return ranges


def merge(ranges):
    if not ranges:
        return []
    ranges.sort()
    out = []
    cur_start, cur_end = ranges[0][0], ranges[0][1]
    for start, end in ranges[1:]:
        if start <= cur_end + 1:
            cur_end = max(cur_end, end)
        else:
            out.append((cur_start, cur_end))
            cur_start, cur_end = start, end
    out.append((cur_start, cur_end))
    return out


def in_range(cp, ranges):
    for start, end in ranges:
        if cp < start:
            return False
        if cp <= end:
            return True
    return False


def intersect(a_ranges, b_ranges):
    """Return merge'd ranges of codepoints present in both range lists."""
    # Both inputs may be unsorted (e.g. DerivedGeneralCategory.txt groups
    # entries by category rather than by codepoint), so sort them first.
    b_sorted = sorted(b_ranges)
    result = []
    for start, end in a_ranges:
        for cp in range(start, end + 1):
            if in_range(cp, b_sorted):
                result.append((cp, cp))
    return merge(result)


def output_table(name, ranges):
    print(f'static const struct LineBreakAuxRange {name}[] = {{')
    for start, end in ranges:
        print(f'\t{{0x{start:X}, 0x{end:X}}},')
    print('};')
    print()


def main():
    gc_file = sys.argv[1] if len(sys.argv) > 1 else \
        'DerivedGeneralCategory.txt'
    lb_file = sys.argv[2] if len(sys.argv) > 2 else 'LineBreak.txt'
    emoji_file = sys.argv[3] if len(sys.argv) > 3 else 'emoji-data.txt'

    gc = load_ranges(gc_file)
    lb = load_ranges(lb_file)
    emoji = load_ranges(emoji_file)

    sa = [(s, e) for s, e, p in lb if p == 'SA']
    qu = [(s, e) for s, e, p in lb if p == 'QU']
    mn_mc = [(s, e) for s, e, p in gc if p in ('Mn', 'Mc')]
    pi = [(s, e) for s, e, p in gc if p == 'Pi']
    pf = [(s, e) for s, e, p in gc if p == 'Pf']
    cn = [(s, e) for s, e, p in gc if p == 'Cn']
    extpict = [(s, e) for s, e, p in emoji if p == 'Extended_Pictographic']

    sa_cm = intersect(sa, mn_mc)
    pi_qu = intersect(qu, pi)
    pf_qu = intersect(qu, pf)
    pot_emoji = intersect(extpict, cn)

    with open(gc_file, 'r', encoding='utf-8') as f:
        gc_head = f.readline() + f.readline()
    with open(lb_file, 'r', encoding='utf-8') as f:
        lb_head = f.readline() + f.readline()
    with open(emoji_file, 'r', encoding='utf-8') as f:
        emoji_head = f.readline() + f.readline()

    print('/* The content of this file is generated from:')
    print(gc_head, end='')
    print(lb_head, end='')
    print(emoji_head, end='')
    print('*/')
    print()
    print('#include "linebreakdef.h"')
    print()
    output_table('lb_prop_pi_qu', pi_qu)
    output_table('lb_prop_pf_qu', pf_qu)
    output_table('lb_prop_sa_cm', sa_cm)
    output_table('lb_prop_potential_emoji', pot_emoji)


if __name__ == '__main__':
    main()
