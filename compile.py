#!/usr/bin/env python3
from PIL import Image
import sys
import argparse

GLYPH_SCALE       = 4
GLYPH_IN_WIDTH    = 5
GLYPH_IN_HEIGHT   = 7
GLYPH_OUT_WIDTH  = GLYPH_IN_WIDTH * GLYPH_SCALE
GLYPH_OUT_HEIGHT = GLYPH_IN_HEIGHT * GLYPH_SCALE
GLYPH_PIX_BORDER = 1

parser = argparse.ArgumentParser()
parser.add_argument('input')
args = parser.parse_args()

bit_idx = 0

def drip(value):
    global bit_idx

    if bit_idx == 0:
        sys.stdout.write('0b')
    bit_idx = (bit_idx + 1) % 8

    sys.stdout.write(str(value))
    if bit_idx == 0:
        sys.stdout.write(',')

def pad_eol():
    global bit_idx

    padding = (8 - bit_idx) % 8
    for i in range(padding):
        drip(0)
    sys.stdout.write('\n\t')

image = Image.open(args.input)

def drip_row(glyph_x, row):
    global image
    for i in range(GLYPH_IN_WIDTH):
        for j in range(GLYPH_SCALE - GLYPH_PIX_BORDER):
            drip(image.getpixel((glyph_x + i, row)))
        for j in range(GLYPH_PIX_BORDER):
            drip(0)
    pad_eol()

glyph_idx = 0
num_glyphs = image.width // GLYPH_IN_WIDTH

name_sans_suffix = args.input.split('.')[0]
sys.stdout.write(f'static const uint8_t {name_sans_suffix}[] =\n\t{{\n\t')
for g in range(num_glyphs):
    sys.stdout.write(f'/* Glyph {g} */\n\t')
    glyph_x = g * GLYPH_IN_WIDTH
    for y in range(GLYPH_IN_HEIGHT):
        for j in range(GLYPH_SCALE - GLYPH_PIX_BORDER):
            drip_row(glyph_x, y)

        for j in range(GLYPH_PIX_BORDER):
            for g in range(GLYPH_OUT_WIDTH):
                drip(0)
            pad_eol()

print('};')
