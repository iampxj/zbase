/*
 * Copyright 2024 wtcat
 */

#ifdef CONFIG_HEADER_FILE
#include CONFIG_HEADER_FILE
#endif

#define pr_fmt(fmt) "<sport_addedlist>: "fmt

#include "basework/lib/crc.h"
#include "basework/lib/string.h"
#include "basework/generic.h"
#include "basework/errno.h"
#include "basework/log.h"
#include "basework/malloc.h"

#include "basework/misc/workout_spdata.h"
#include "basework/misc/workout_dataimpl.h"
#include "basework/misc/workout_sportaddedlist.h"
#include "basework/misc/workout_sporttype.h"

static const uint8_t default_sports[] = {
    K_SPORT_ORUN,
    K_SPORT_OWALK,
    K_SPORT_FSTRAINING,
    K_SPORT_OCYCLE,
    K_SPORT_AEROBICS,
    K_SPORT_PSWIM,
    K_SPORT_IRUN,
    K_SPORT_HIIT,
    K_SPORT_BARBELL,
    K_SPORT_KICKBOXING,
    K_SPORT_ICYCLE,
    K_SPORT_IWALK,
    K_SPORT_YOGA,
    K_SPORT_ELLIPTICAL,
    K_SPORT_HIKING,
    K_SPORT_PADEL,
    K_SPORT_COOLDOWN,
    K_SPORT_CANOEING,
    K_SPORT_CROSS_COUNTRY_SKIING
};

struct sport_recdata *sport_addedlist_get(void) {
    struct spdata_object *o;
    o = workout_spdata_find("sport_addedlist");
    if (o != NULL)
        return o->private;
    return NULL;
}

int sport_addedlist_delete(struct sport_recdata *sr, uint8_t type) {
    SportAddedList *list = (SportAddedList *)sr->data;

    if (type >= MAX_SPORTS || list->count == 0)
        return -EINVAL;

    for (int i = 0; i < (int)list->count; i++) {
        if (list->nodes[i].type == type) {
            int n = (int)list->count - i - 1;
            if (n > 0) {
                RTE_FOR16_EXECUTE(n, {
                    list->nodes[i] = list->nodes[i + 1];
                    i++;
                });
            }
            list->nodes[list->count - 1].type = 0;
            list->count--;
            break;
        }
    }
    return 0;
}

int sport_addedlist_update(struct sport_recdata *sr, uint8_t type) {
    SportAddedList *list = (SportAddedList *)sr->data;
    SportNode front;
    int index;

    if (type >= MAX_SPORTS || list->count == 0)
        return -EINVAL;

    /*
     * Move the sport type to front
     */
    for (index = list->count - 1; index >= 0; index--) {
        if (list->nodes[index].type == type) {
            front = list->nodes[index];
            if (index != 0) {
                RTE_FOR16_EXECUTE(index, {
                    list->nodes[index] = list->nodes[index - 1];
                    index--;
                });
                list->nodes[0] = front;
                goto _exit;
            }
        }
    }

    /*
     * Add a new sport type
     */
    index = list->count;
    RTE_FOR16_EXECUTE(index, {
        list->nodes[index] = list->nodes[index - 1];
        index--;
    });
    list->nodes[0].type = type;
    list->count++;

_exit:
    sr->dirty = 1;
    return 0;
}

static int sport_nodes_restore(struct spdata_object *o, void *p) {
    SportAddedList *sl = p;
    int i;
    
    for (i = 0; i < rte_array_size(default_sports); i++)
        sl->nodes[i].type = default_sports[i];

    strlcpy(sl->header.name, o->name, SPORT_DATNAME_LEN);
    sl->header.chksum = 0;
    sl->count  = (uint16_t)i;
    return 0;
}

WORKOUT_DATA_RECORD(
    sport_addedlist, 
    "splist.dat", 
    sizeof(SportAddedList), 
    sport_nodes_restore
)
