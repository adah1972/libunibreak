#!/usr/bin/env python
import sys

lines = sys.stdin.readlines()
lines_out = sorted(lines, key=lambda line: int(line.split("..")[0], 16))
sys.stdout.writelines(lines_out)
