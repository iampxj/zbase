#!/usr/bin/python

import sys
import os
import json

partition_list = []
partition_ver = ''

def parse_gpt(file):
    with open(file, 'r') as fin:
        js = json.load(fin)
        version = js['version']
        devices = js['devices']

        global partition_list
        global partition_ver
        partition_ver = version

        for dev in devices:
            capacity = int(dev['capacity'], 16)
            partitions = dev['partitions']
            offset = 0
            size = 0
            last_offset = 0
            last_size = 0
            total_size = 0

            for pt in partitions:
                offset = int(pt['offset'], 16)
                size   = int(pt['size'], 16)
                if (last_offset + last_size > offset) or (offset + size > capacity):
                    print('***Error: partition offset error: ', pt['label'])
                    return
                if size % 4096 != 0:
                    print('***Error: partition size error: ', pt['label'])
                    return

                if pt['label'] == 'picture':
                    partition_list.append(('res.bin', offset, size))
                elif pt['label'] == 'font':
                    partition_list.append(('fonts.bin', offset, size))
                elif pt['label'] == 'watchface':
                    partition_list.append(('udisk.bin', offset, size))

                last_offset = offset
                last_size   = size
                total_size  += size
            
            if total_size > capacity:
                print("***Error: Partitions is too large!! ", dev['name'])
        fin.close()

def merge_bin(dir):
    offset = 0
    dst_path = os.path.join(dir, 'ui.bin')

    print('GPT Version: ', partition_ver)
    with open(dst_path, 'wb') as f:
        for pt in partition_list:
            src_path = os.path.join(dir, pt[0])
            filesize = os.path.getsize(src_path)
            if filesize > pt[2]:
                print('***Error: file({}) is large than partition size({})'.format(src_path, hex(pt[2])))
                return
            
            with open(src_path, 'rb') as frd:
                f.seek(offset)
                f.write(frd.read(-1))
                frd.close()
            
            print('  => name({}) offset({}) size({}) binofs({})'.format(pt[0], hex(pt[1]), hex(pt[2]), hex(offset)))

            # partition size
            offset += pt[2]
    print('Generate binary: {} size({})'.format(dst_path, hex(offset)))

def main(argv):
    if len(argv) != 2:
        print('{} [directory]'.format(argv[0]))
        return

    dir = argv[1]
    print('GPT Build dir: ', dir)
    parse_gpt(os.path.join(dir, 'partition.json'))
    merge_bin(dir)

if __name__ == "__main__":
    main(sys.argv)