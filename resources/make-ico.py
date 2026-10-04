"""Packs glimpse.ico from a large rendering of glimpse.svg.

    python make-ico.py glimpse-1024.png glimpse.ico

Render the SVG first with any SVG renderer at 1024x1024 on a transparent
background (e.g. inkscape glimpse.svg -w 1024 -o glimpse-1024.png).
Needs Pillow.
"""
import sys

from PIL import Image

SIZES = [16, 20, 24, 32, 40, 48, 64, 128, 256]

source = Image.open(sys.argv[1]).convert('RGBA')
frames = [source.resize((size, size), Image.LANCZOS) for size in SIZES]
frames[-1].save(sys.argv[2], sizes=[(size, size) for size in SIZES], append_images=frames[:-1])
