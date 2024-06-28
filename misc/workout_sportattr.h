/*
 * Copyright 2024 wtcat
 */
#ifndef WORKOUT_SPORT_ATTR_H_
#define WORKOUT_SPORT_ATTR_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C"{
#endif

#ifndef BIT
#define BIT(n) (0x1ul << (n))
#endif

typedef enum {
    K_SPORT_ATTR_WORKOUT_MODE_CUSTOM          = BIT(0),
    K_SPORT_ATTR_WORKOUT_MODE_TARGET_TIME     = BIT(1),
    K_SPORT_ATTR_WORKOUT_MODE_TARGET_DISTANCE = BIT(2),
    K_SPORT_ATTR_WORKOUT_MODE_TARGET_CALORIE  = BIT(3),
    K_SPORT_ATTR_WORKOUT_MODE_AI_PACER        = BIT(4),
    K_SPORT_ATTR_WORKOUT_MODE_CLASSES         = BIT(5),
    K_SPORT_ATTR_WORKOUT_MODE_OPEN            = BIT(6)
} SportWorkoutModeAttribute;

typedef enum {
    K_SPORT_ATTR_LAYOUT_CALORIE_WORKOUT       = BIT(0),
    K_SPORT_ATTR_LAYOUT_CALORIE_TOTAL         = BIT(1),
    K_SPORT_ATTR_LAYOUT_HEARTRATE_REALTIME    = BIT(2),
    K_SPORT_ATTR_LAYOUT_HEARTRATE_MAX         = BIT(3),
    K_SPORT_ATTR_LAYOUT_HEARTRATE_AVG         = BIT(4),
    K_SPORT_ATTR_LAYOUT_ALTITUDES_CLIMB       = BIT(5),
    K_SPORT_ATTR_LAYOUT_ALTITUDES_REALTIME    = BIT(6),
    K_SPORT_ATTR_LAYOUT_DISTANCE_TOTAL        = BIT(7),
    K_SPORT_ATTR_LAYOUT_DISTANCE_SEGMENT      = BIT(8),
    K_SPORT_ATTR_LAYOUT_PACE_REALTIME         = BIT(9),
    K_SPORT_ATTR_LAYOUT_PACE_AVG              = BIT(10),
    K_SPORT_ATTR_LAYOUT_PACE_SEGMENT          = BIT(11),
    K_SPORT_ATTR_LAYOUT_PACE_MAX              = BIT(12),
    K_SPORT_ATTR_LAYOUT_STEPRATE_REALTIME     = BIT(13),
    K_SPORT_ATTR_LAYOUT_STEPRATE_AVG          = BIT(14),
    K_SPORT_ATTR_LAYOUT_SPEED_REALTIME        = BIT(15),
    K_SPORT_ATTR_LAYOUT_SPEED_AVG             = BIT(16),
    K_SPORT_ATTR_LAYOUT_SPEED_SEGMENT         = BIT(17),
    K_SPORT_ATTR_LAYOUT_SPEED_MAX             = BIT(18),
    K_SPORT_ATTR_LAYOUT_DURATION_TOTAL        = BIT(19),
    K_SPORT_ATTR_LAYOUT_DURATION_SEGMENT      = BIT(20),
    K_SPORT_ATTR_LAYOUT_UNIT                  = BIT(21),
} SportViewLayoutAttribute;

typedef enum {
    K_SPORT_ATTR_REMIND_PACE_REALTIME         = BIT(0),
    K_SPORT_ATTR_REMIND_HEARTRATE             = BIT(1),
    K_SPORT_ATTR_REMIND_STEPRATE_REALTIME     = BIT(2),
    K_SPORT_ATTR_REMIND_POWER_REALTIME        = BIT(3),
    K_SPORT_ATTR_REMIND_SPEED_REALTIME        = BIT(4),
    K_SPORT_ATTR_REMIND_NONE                  = BIT(5),
    K_SPORT_ATTR_REMIND_TIME                  = BIT(6),
    K_SPORT_ATTR_REMIND_AUTOLAP               = BIT(7),
    K_SPORT_ATTR_REMIND_AUTOPAUSE             = BIT(8),
} SportRemindAttribute;

typedef enum {
    K_SPORT_ATTR_REMIND_CUSTOM_PACE_REALTIME  = BIT(0),
    K_SPORT_ATTR_REMIND_CUSTOM_PACE_AVG       = BIT(1),
    K_SPORT_ATTR_REMIND_CUSTOM_HEARTRATE      = BIT(2),
    K_SPORT_ATTR_REMIND_CUSTOM_STEPRATE       = BIT(3),
    K_SPORT_ATTR_REMIND_CUSTOM_POWER_REALTIME = BIT(4),
    K_SPORT_ATTR_REMIND_CUSTOM_POWER_AVG      = BIT(5),
    K_SPORT_ATTR_REMIND_CUSTOM_SPEED_REALTIME = BIT(6),
    K_SPORT_ATTR_REMIND_CUSTOM_SPEED_AVG      = BIT(7),
    K_SPORT_ATTR_REMIND_CUSTOM_NONE           = BIT(8),
} SportRemindCustomAttribute;

typedef enum {
    K_SPORT_ATTR_SUM_TRACK                    = BIT(0), 
    K_SPORT_ATTR_SUM_DURATION                 = BIT(1), 
    K_SPORT_ATTR_SUM_PACE_AVG                 = BIT(2), 
    K_SPORT_ATTR_SUM_PACE_AVG100              = BIT(3), 
    K_SPORT_ATTR_SUM_SPEED_AVG                = BIT(4), 
    K_SPORT_ATTR_SUM_CALORIE                  = BIT(5), 
    K_SPORT_ATTR_SUM_HEARTRATE_AVG            = BIT(6), 
    K_SPORT_ATTR_SUM_POWER_AVG                = BIT(7), 
    K_SPORT_ATTR_SUM_PACE_TABLE               = BIT(8), 
    K_SPORT_ATTR_SUM_SPEED_TABLE              = BIT(9),
    K_SPORT_ATTR_SUM_ALTITUDES_TABLE          = BIT(10), 
    K_SPORT_ATTR_SUM_HEARTRATE_TABLE          = BIT(11), 
    K_SPORT_ATTR_SUM_HEARTRATE_HISTOGRAM      = BIT(12), 
    K_SPORT_ATTR_SUM_SHOW                     = BIT(13), 
} SportSumAttribute;

typedef enum {
    K_SPORT_ATTR_PREFER_SINGLELAP             = BIT(0),
    K_SPORT_ATTR_PREFER_HEARTRATE_RANG        = BIT(1),
    K_SPORT_ATTR_PREFER_PACE                  = BIT(2),
    K_SPORT_ATTR_PREFER_TRACK                 = BIT(3),
} SportPreferViewAttribute;

typedef enum {
    K_SPORT_ATTR_DATA_DISTANCE_TOTAL          = BIT(0),
    K_SPORT_ATTR_DATA_HEARTRATE_REALTIME      = BIT(1),
    K_SPORT_ATTR_DATA_PACE_REALTIME           = BIT(2),
    K_SPORT_ATTR_DATA_PACE_AVG                = BIT(3),
    K_SPORT_ATTR_DATA_STEPRATE_REALTIME       = BIT(4),
    K_SPORT_ATTR_DATA_CALORIE                 = BIT(5),
} SportDataItemAttribute;

uint32_t sport_get_workout_mode_attribute(int type);
uint32_t sport_get_view_layout_attribute(int type);
uint32_t sport_get_remind_attribute(int type);
uint32_t sport_get_remind_custom_attribute(int type);
uint32_t sport_get_sum_attribute(int type);
uint32_t sport_get_prefer_view_attribute(int type);
uint32_t sport_get_dataitem_attribute(int type, int nitem);

#ifdef __cplusplus
}
#endif
#endif /* WORKOUT_SPORT_ATTR_H_ */
