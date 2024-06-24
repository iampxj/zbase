/*
 * Copyright 2023 wtcat
 */
#ifndef BASEWORK_DEV_INPUT_H_
#define BASEWORK_DEV_INPUT_H_

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C"{
#endif

enum input_state {
    INPUT_STATE_RELEASED = 0,
    INPUT_STATE_PRESSED
};

struct point_value {
    int16_t x;
    int16_t y;
};

struct input_event {
    struct point_value point; 
    uint32_t key;
    uint32_t button; /* button ID */
    int16_t  encdiff; 
    enum input_state state;
    bool continue_reading;
};


#ifdef __cplusplus
}
#endif
#endif /* BASEWORK_DEV_INPUT_H_ */
