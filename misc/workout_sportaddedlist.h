/*
 * Copyright 2024 wtcat
 */
#ifndef WORKOUT_SPORTADDEDLIST_H_
#define WORKOUT_SPORTADDEDLIST_H_

#include "basework/misc/workout_dataimpl.h"

#ifdef __cplusplus
extern "C"{
#endif

typedef struct {
    uint8_t  type;
} SportNode;

typedef struct {
    SportDataHeader header;
    uint16_t        count;
    SportNode       nodes[MAX_SPORTS];
} SportAddedList;

struct sport_recdata *sport_addedlist_get(void);
int sport_addedlist_update(struct sport_recdata *sr, uint8_t type);
int sport_addedlist_delete(struct sport_recdata *sr, uint8_t type);

#ifdef __cplusplus
}
#endif
#endif /* WORKOUT_SPORTADDEDLIST_H_ */
