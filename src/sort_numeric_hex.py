#!/usr/bin/env python
import sys

lines = open(sys.argv[1]).readlines()
lines_out = sorted(lines, key=lambda line: int(line.split("..")[0], 16))
map(sys.stdout.write, lines_out)
