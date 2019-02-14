# !/usr/bin/env python
#
# Copyright 2016-2017 Benjamin Vanheuverzwijn <bvanheu@gmail.com>
#           2016-2017 Marc-Etienne M. Leveille <marc.etienne.ml@gmail.com>
#           2019 Michael Jeanson <mjeanson@gmail.com>
#           2019 Thomas Dupuy <thom4s.d@gmail.com>
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.
#

import argparse
import os
import sys
import pycparser
from PIL import Image

# struct bitmap_t {
#     const uint8_t *image;
#     uint32_t width;
#     uint32_t height;
#     enum image_encoding encoding;
#     uint16_t bg_color;
# } bitmap;
# enum image_encoding {
    # image_encoding_1bit
    # image_encoding_565bits
# };

H_TEMPLATE = """/*
 * This file was automatically generated by """ + __file__ + """
 */

#ifndef {var_name:s}_h
#define {var_name:s}_h

#include <stdint.h>
#include <bitmap.h>

__attribute__((weak))
uint8_t {var_name:s}_bytes[] = {{
    {byte_array:s}
}};

__attribute__((weak))
struct bitmap {var_name:s} = (struct bitmap){{
    .image={var_name:s}_bytes,
    .width={width:d},
    .height={height:d},
    .encoding=image_encoding_{encoding:s},
    .bg_color=0
}};

#endif /* {var_name:s}_h */
"""

def RGBA_to_RGB888(image):
    """ _in: tuple(RGBA)
        _out: list(tuple(RGB))
    """
    return ((r, g, b) for r, g, b, a in image.getdata())


def RGB888_to_RGB565(image):
    """ _in: list(tuple(RGB888))
        _out: list(RGB565)
        rrrrrggggggbbbbb
    """
    _bytes = []
    for red, green, blue in image:
        red = red >> 3
        green = green >> 2
        blue = blue >> 3
        merged = (red << 11) | (green << 5) | blue
        h = merged >> 8
        l = merged & 0xff
        # TODO: do something better
        _bytes.append(hex(h))
        _bytes.append(hex(l))
    return ','.join(_bytes)


def encode_image_BW(image):
    if sys.version_info >= (3, 0):
        return ','.join('0x{:02x}'.format(b) for b in image.tobytes())
    else:
        return ','.join('0x{:02x}'.format(ord(b)) for b in image.tobytes())


def encode_image_RGBA(image):
    image_RGB888 = RGBA_to_RGB888(image)
    return RGB888_to_RGB565(image_RGB888)


def encode_image(input_file_path, output_file_path, rotate=False):
    image = Image.open(input_file_path)
    if image.mode == '1':
        encoding = '1bit'
        if rotate:
            image = image.transpose(Image.ROTATE_270)
        array = encode_image_BW(image)
    elif 'RGBA' in image.mode:
        encoding = '565bits'
        if rotate:
            image = image.transpose(Image.ROTATE_270)
        array = encode_image_RGBA(image)
    else:
        sys.exit("Image must be in 1-bit or 2-bit color mode ({}).".format(image.mode))

    args = {
        "byte_array":array,
        "var_name":os.path.splitext(os.path.basename(output_file_path))[0],
        "width":image.width,
        "height":image.height,
        "encoding":encoding
    }

    with open('{}.h'.format(output_file_path), "w") as f:
        f.write(H_TEMPLATE.format(**args))


def decode_image(input_file_path, output_file_path, width, rotate=False):
    tree = pycparser.parse_file(input_file_path, use_cpp=True, cpp_path="arm-none-eabi-cpp")
    if len(tree.ext) != 1:
        raise Exception("C file has more than one element.")
    font = tree.ext[0]
    if not "char" in font.type.type.type.names:
        raise Exception("Array must be of type char.")
    buf = b""
    for con in font.init.exprs:
        buf += chr(int(con.value, 0))
    height = (len(buf) * 8) / width
    image = Image.frombytes("1", (width, height), buf)
    if rotate:
        image = image.transpose(Image.ROTATE_90)
    image.save(output_file_path)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description='Transform images from 1 color\
        bitmap to C array and vice-versa.')

    parser.add_argument('-d', '--decode', action='store_true',
                        help='Decode C file to image')
    parser.add_argument('-w', '--width', type=int,
                        help='Width of the image to decode')
    parser.add_argument('-r', '--rotate', action='store_true',
                        help='Rotate image by 90 degrees')
    parser.add_argument('infile', help='Input file')
    parser.add_argument('outfile', help='Output file')

    args = parser.parse_args()

    if args.decode:
        if args.width is None:
            parser.print_help(sys.stderr)
            sys.stderr.write("\nerror: Specify the width of the image to decode.\n")
            sys.exit(1)

        decode_image(args.infile, args.outfile, args.width, args.rotate)

    else:
        encode_image(args.infile, args.outfile, args.rotate)
