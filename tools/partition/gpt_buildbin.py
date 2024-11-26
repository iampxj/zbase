#!/usr/bin/python

import sys
import os
import json

partition_list = []
partition_ver = ''
target_device = ''

def parse_gpt(file):
    with open(file, 'r') as fin:
        js = json.load(fin)
        version = js['version']
        devices = js['devices']
        if 'target' in js:
            global target_device
            target_device = js['target']

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
                
                if 'bin' in pt:
                    partition_list.append((pt['bin'], offset, size))

                last_offset = offset
                last_size   = size
                total_size  += size
            
            if total_size > capacity:
                print("***Error: Partitions is too large!! ", dev['name'])
        fin.close()

def merge_bin(dir):
    offset = 0
    dst_path = os.path.join(dir, 'gpdata.bin')

    print('GPT Version: ', partition_ver)
    with open(dst_path, 'wb') as f:
        for pt in partition_list:
            src_path = os.path.join(dir, pt[0])
            if os.path.exists(src_path):
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
            else:
                offset += pt[2]
                print('file({}) is not exist'.format(pt[0]))

    print('Generate binary: {} size({}) device({})'.format(dst_path, hex(offset), target_device))

def main(argv):
    if len(argv) < 2:
        print('{} [file] [[stdout]]'.format(argv[0]))
        return
    
    if (len(argv) == 3):
        sys.stdout = argv[2]
        
    file = argv[1]
    print('GPT file: ', file)
    parse_gpt(file)
    merge_bin(os.path.dirname(file))
    if sys.stdout != sys.__stdout__:
        sys.stdout = sys.__stdout__

if __name__ == "__main__":
    main(sys.argv)