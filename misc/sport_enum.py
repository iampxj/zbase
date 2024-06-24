#!/usr/bin/python

# Target file: ini
# [english]
# ARCHERY = 108
# ALPINE_SKIING = 111
# AEROBICS = 26

import sys
from configparser import ConfigParser


c_sport_enum = '''
typedef enum {{
{sport_list}
}} SportType;
'''

c_sport_sort_lang = '''
static const unsigned char {lang}_lang_sports[] = {{
{sport_list}
}};
'''

c_sport_array = '''
#if 0
static const unsigned char *sorted_sports[/*lang*/] = {{
{array_items}
}};
#endif
'''

c_sport_array_item = ''
c_source_code = ''

def gen_c_enum_sport(section):
    sport_dict = {}
    enum_express = ''
    max_value = 0

    # Rename sport type
    for key in section:
        name = 'K_SPORT_' + key.upper()
        value = section.getint(key)
        sport_dict[name] = value
        if value > max_value:
            max_value = value

    # Sort sport
    sport_sorted = sorted(sport_dict.items(), key=lambda s:s[1])
    for items in sport_sorted:
        strval = str(items[1])
        enum_str = '    {name} = {val},\n'.format(name=items[0], val=strval)
        enum_express += enum_str

    enum_str = '    K_SPORT_MAX = {val}\n'.format(val = str(max_value + 1))
    enum_express += enum_str

    global c_source_code
    c_source_code += c_sport_enum.format(
                        sport_list = enum_express
                        )

def gen_c_sorted_array(config, name):
    section = config[name]
    c_express = ''

    for key in section:
        value = section.getint(key)
        strval = str(value)
        c_express += '    ' + strval + ',\n'

    global c_source_code
    c_source_code += c_sport_sort_lang.format(
                        lang = name, 
                        sport_list = c_express)

def gen_c_array_items(name):
    global c_sport_array_item
    c_sport_array_item += name + '_lang_sports,\n'
    
    
def parse_ini(filename):
    config = ConfigParser()
    config.read(filename, encoding='utf-8')
    
    # Generate sport type
    
    enum_ok = False
    for section in config.sections():
        if not enum_ok:
            enum_ok = True
            gen_c_enum_sport(config[section])

        gen_c_sorted_array(config, section)
        gen_c_array_items(section)

    global c_source_code
    c_source_code += c_sport_array.format(
        array_items = c_sport_array_item
    )

    # Generate c source file
    with open('sport_auto_generate.c', 'w') as f:
        f.write(c_source_code)
        f.close()


def main(argv):
    parse_ini(argv[1])


if __name__ == "__main__":
    main(sys.argv)