#!/usr/bin/python

import sys

c_xxxx_array = '''
static const {utype} sport_xxxx_bitmap[] = {{
{value}
}};
'''

c_xxxx_type = 'x'

def line_parse(buf):
    lstr = buf.split(',')

    global c_xxxx_type
    if c_xxxx_type == 'x':
        if len(lstr) > 16:
            c_xxxx_type = 'uint32_t'
        elif len(lstr) > 8:
            c_xxxx_type = 'uint16_t'
        else:
            c_xxxx_type = 'uint8_t'

    shift = 0
    value = 0
    for elem in lstr:
        if elem[0] == 'Y':
            value = value | (1 << shift)
        shift += 1
    return value

def file_parse(path):
    array_content = ''
    with open(path, 'r') as f:
        lines_buffer = f.readlines()
        for line in lines_buffer:
            value = line_parse(line)
            vstr = '    0x{:02x},\n'.format(value)
            array_content += vstr

        cpath = path + '.c'
        with open(cpath, 'w') as fo :
            c_source = c_xxxx_array.format(
                        utype = c_xxxx_type, 
                        value = array_content
                        )
            fo.write(c_source)
            fo.close()

        f.close()
        
def main(argv):
    file_parse(argv[1])

if __name__ == "__main__":
    main(sys.argv)
