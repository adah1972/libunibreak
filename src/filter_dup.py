#!/usr/bin/env python
from __future__ import print_function
import sys

last_first = None
last_second = None
last_value = None
for line in sys.stdin:
    first, second = line.rstrip().split('..')
    second, value = second.split(';')
    value = value.lstrip()
    first = int(first, 16)
    second = int(second, 16)
    if last_value is None:
        last_first = first
        last_second = second
        last_value = value
    elif last_second + 1 != first or last_value != value:
        print('{:04X}..{:04X}; {}'.format(last_first, last_second, last_value))
        last_first = first
        last_second = second
        last_value = value
    else:
        last_second = second

print('{:04X}..{:04X}; {}'.format(last_first, last_second, last_value))
