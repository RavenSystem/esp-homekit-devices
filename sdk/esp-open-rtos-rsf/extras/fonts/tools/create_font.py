#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import sys, os
from PIL import Image, ImageFont, ImageDraw
import argparse
import jinja2
import re
import time

def gen_char(index, c, im):
    bw = (im.size[0] + 7) // 8
    res = {
        'index': index,
        'code': c,
        'offset': bw * im.size[1] * index,
        'rows': []
    }
    
    data = tuple(im.getdata())
    
    for row in range(im.size[1]):
        r = {
            'data': [],
            'asc': [],
        }
        for b in range(bw):
            byte = 0
            for i in range(8):
                idx = b * 8 + i
                bit = data[row * im.size[0] + idx] if idx < im.size[0] else 0
                if bit:
                    byte |= 1
                    r['asc'].append('#')
                else:
                    r['asc'].append('.')
                byte <<= 1
            r['data'].append(byte >> 1)
            
        r['asc'] = ''.join(r['asc'])
        res['rows'].append(r)
        
    return res
        

def main(args):
    fnt = ImageFont.load(args.font)
    size = fnt.getsize('A')
    
    im = Image.new('RGB', size)
    draw = ImageDraw.Draw(im)
    
    if args.last - args.first < 1:
        raise ValueError('Invalid --first or --last')
    
    chars = []
    for idx in range(args.last - args.first + 1):
        draw.rectangle(((0, 0), size), fill = 0)
        draw.text((0, 0), chr(idx + args.first), font=fnt)
        chars.append(gen_char(idx, idx + args.first, im.convert('1')))
        
    env = jinja2.Environment(loader=jinja2.FileSystemLoader(os.path.dirname(os.path.abspath(__file__))), finalize=lambda x: '' if x is None else x)
    print(env.get_template(args.template).render({
        'font': {
            'name': args.name,
            'size': size,
            'charset': args.charset,
            'first': args.first,
            'last': args.last,
        },
        'chars': chars,
        'created': time.ctime()
    }))


_CLEAN_RE = re.compile(r'[^a-z0-9_]', re.I)

def clean_str(s):
    return _CLEAN_RE.sub('_', s)

if __name__ == "__main__":
    parser=argparse.ArgumentParser(description='Fixed fonts converter')
    parser.add_argument('-f', '--font', type=str, required=True, help='PIL font filename')
    parser.add_argument('-n', '--name', type=clean_str, required=True, help='Font name')
    parser.add_argument('-c', '--charset', type=clean_str, required=True, help='Charset')
    parser.add_argument('--first', type=int, help='First character', default=1)
    parser.add_argument('--last', type=int, help='Last character', default=255)
    parser.add_argument('-t', '--template', type=str, help='Template filename', default='template.c')
    main(parser.parse_args(sys.argv[1:]))

