#!/usr/bin/python
# -*- coding: UTF-8 -*-

# Target file: txt
# bàng qiú	BASEBALL = 52
# bèng jí	BUNGEE_JUMPING = 83
# bì qiú	SQUASH = 51
# bā lěi	BALLET = 60
# bīng hú	CURLING = 71

import sys
import os
import xlrd

c_source_header = ''

c_source_enum = '''
typedef enum {{
{sport_list}
}} SportType;
'''

c_source_head = '''
/*
 * Language: {lang}
 */
struct _{lang}_sorted_list {{
    const uint8_t      *types;
    uint16_t            typecnt;
    uint16_t            itemcnt;
    SportSortedItem     items[{itemcnt}];
}};
'''

c_sport_types = '''
static const uint8_t {lang}_lang_types[] = {{
{types_list}
}};
'''

c_sports_sorted = '''
static const struct _{lang}_sorted_list {lang}_lang_sports = {{
    .types    = {lang}_lang_types,
    .typecnt  = sizeof({lang}_lang_types)/sizeof({lang}_lang_types[0]),
    .itemcnt  = {itemcnt},
    .items    = {{
        {items}
    }}
}};

const SportSortedList *{lang}_sport_get_sortedlist(void) {{
    return (const SportSortedList *)&{lang}_lang_sports;
}}
'''

c_sports_sorted_item = '''
        {{ "{code}", {count}, {offset} }},'''

        


c_export_symbol = '''
const SportSortedList *{lang}_sport_get_sortedlist(void);'''

c_source_code = c_source_header

global_dict = {}

column_list = [
    ('澳式足球', '中文', 'ch'), 
    ('射箭',     '英文', 'en'), 
    ('钓鱼',     '德语', 'ge'), 
    ('赛艇',     '法语', 'fr'), 
    ('射箭',     '西班', 'sp'), 
    ('田径',     '日语', 'jp'), 
    ('极限飞盘', '俄语', 'ru'), 
    ('田径',     '葡萄', 'po'), 
    ('放风筝',   '意大', 'it')
]

def generate_c_source():
    with open('sport_langtype.c', 'w') as f:
        f.write(c_source_code)
        f.close()

def parse_enumtype(maptree):
    sport_dict = {}
    enum_express = ''
    max_value = 0

    for name, value in maptree.values():
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
    c_source_code += c_source_enum.format(
                        sport_list = enum_express
                        )

def parse_language(lang, table, first_row, first_col):
    count = 0
    offset = 0
    item_count = 0
    oldkey = '*'
    key = '*'
    data_item = ''
    type_item = ''
    data_str = ''

    row = first_row
    while row < table.nrows:
        langkey = table.cell(row, first_col).value
        charkey = table.cell(row, first_col+1).value

        if langkey in global_dict.keys():
            (typename, tid) = global_dict[langkey]
            key       = charkey[0]
            vstr      = str(tid)
            type_item += '\t' + vstr + ',\n'

            if key != oldkey:
                if count > 0:
                    data_item += c_sports_sorted_item.format(
                                            code  = oldkey,
                                            count = count,
                                            offset = offset
                                            )
                    data_str = ''
                    offset += count
                    count = 0
                item_count += 1
                oldkey = key
            
            data_str += vstr + ','
            count += 1
        else:
            print('Invalid sport type({}): {}'.format(lang, langkey))
        row += 1

    # The last line
    data_item += c_sports_sorted_item.format(
                    code  = key,
                    count = count,
                    offset = offset
                    )

    global c_source_code
    c_source_code += c_source_head.format(
        lang    = lang,
        itemcnt = item_count
    )
    c_source_code += c_sport_types.format(
                    lang       = lang,
                    types_list = type_item
                    )
    c_source_code += c_sports_sorted.format(
                        lang    = lang,
                        itemcnt = item_count,
                        items   = data_item
                        )

def build_dict(table, first_row):
    global global_dict
    row = first_row
    index = 0

    while row < table.nrows:
        chinese_name = table.cell(row, 0).value
        english_name = table.cell(row, 1).value
        sport_name = 'K_SPORT_' + english_name.replace(' ', '_').upper()
        global_dict[chinese_name] = (sport_name, index)
        index += 1
        row += 1

def workbook_parse(name, first_row):
    xls = xlrd.open_workbook(name)
    sheet1 = xls.sheet_by_index(0)
    print('sheet name: ', xls.sheet_names())

    export_symbols = ''

    # Build database
    build_dict(sheet1, first_row)

    #Generate sport type
    parse_enumtype(global_dict)

    sheet2 = xls.sheet_by_index(1)
    for a, b, c in column_list:
        row = 1
        col = 0
        while col < sheet2.ncols:
            name = sheet2.cell(row, col).value
            if a == name:
                parse_language(c, sheet2, row, col)
                export_symbols += c_export_symbol.format(lang = c)
                break
            col += 1

    # print(c_source_code)
    generate_c_source()
    print(export_symbols)

def main(argv):
    workbook_parse(argv[1], 1)


if __name__ == "__main__":
    main(sys.argv)