#!/usr/bin/python

# Target file: txt
# bàng qiú	BASEBALL = 52
# bèng jí	BUNGEE_JUMPING = 83
# bì qiú	SQUASH = 51
# bā lěi	BALLET = 60
# bīng hú	CURLING = 71

import sys
import os

c_source_head = '''
#include "basework/misc/workout_dataimpl.h"

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
        {{ "{code}", {count}, {offset} }},
'''

c_source_code = ''

def parse_langtext(path, prefix):
    with open(path, 'r') as fin:
        lines = fin.readlines()
        count = 0
        offset = 0
        item_count = 0
        oldkey = '*'
        key = '*'
        data_item = ''
        type_item = ''
        data_str = ''

        for line in lines:
            llist = line.split('=')
            key   = llist[0][0]
            vstr  = str(int(llist[1]))
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

        # The last line
        data_item += c_sports_sorted_item.format(
                        code  = key,
                        count = count,
                        offset = offset
                        )

        global c_source_code

        c_source_code += c_source_head.format(
            lang    = prefix,
            itemcnt = item_count
        )
        c_source_code += c_sport_types.format(
                        lang       = prefix,
                        types_list = type_item
                        )
        c_source_code += c_sports_sorted.format(
                            lang    = prefix,
                            itemcnt = item_count,
                            items   = data_item
                            )

def main(argv):
    
    base = os.path.basename(argv[1])
    prefix = base.split('.')
    parse_langtext(argv[1], prefix[0])

    outfile = argv[1] + '.c'
    with open(outfile, 'w') as fo:
        fo.write(c_source_code)
        fo.close()


if __name__ == "__main__":
    main(sys.argv)