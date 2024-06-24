/*
 * Copyright 2024 wtcat
 */
#ifndef WORKOUT_DATA_H_
#define WORKOUT_DATA_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C"{
#endif

typedef enum {
    K_WORKOUT_C_OPEN,
    K_WORKOUT_C_AI_PACER,
    K_WORKOUT_C_CUSTOM,
    K_WORKOUT_C_WORKOUTS,
    K_WORKOUT_C_GOAL
} WorkoutCreateType;

typedef enum {
    K_WORKOUT_PP_OTHER,
    K_WORKOUT_PP_WARM_UP,
    K_WORKOUT_PP_REPEATS,
    K_WORKOUT_PP_WORK,
    K_WORKOUT_PP_RECOVERY,
    K_WORKOUT_PP_COOLDOWN,
    K_WORKOUT_PP_ADD,
} WorkoutPipelineType;

typedef enum {
    K_WORKOUT_INVALID,
    K_WORKOUT_RUN,
    K_WORKOUT_HIIT,
    K_WORKOUT_CARDIO,
    K_WORKOUT_STRENGTH,
    K_WORKOUT_CYCLING,
    K_WORKOUT_SWIMMING,
    K_WORKOUT_PILATES,
    K_WORKOUT_YOGA,
    K_WORKOUT_STRETCHING,
    K_WORKOUT_OTHER
} WorkoutType;

typedef enum {
    K_WORKOUT_CATEGORY_EASYRUN,
    K_WORKOUT_CATEGORY_KEEPFIT,
    K_WORKOUT_CATEGORY_BURNFAT,
    K_WORKOUT_CATEGORY_IMPROVE,
} WorkoutCategory;

/*
 * Workout domain private data
 */
typedef struct {
    WorkoutType      type;
    WorkoutCategory  category;
    uint16_t         id;
} WorkoutDomain;

typedef enum {
    K_DURATION_INVALID,
    K_DURATION_TIME,
    K_DURATION_DISTANCE,
    K_DURATION_MANUALLY_LAP,
    K_DURATION_AUTO_LAP,
    K_DURATION_HEARTRATE,
} DurationType;

typedef enum {
    K_INTENSITY_INVALID,
    K_INTENSITY_OPEN,
    K_INTENSITY_PACE,
    K_INTENSITY_PACE_REALTIME,
    K_INTENSITY_CADENCE,
    K_INTENSITY_HEARTRATE,
    K_INTENSITY_POWER,
} IntensityType;

typedef enum {
    K_UNIT_NONE,
    K_UNIT_MINUTE,
    K_UNIT_SECOND,
    K_UNIT_KCAL,
    K_UNIT_REPETITION,
    K_UNIT_SPM,
    K_UNIT_BPM_RESERVE_RATE,
} UnitType;

typedef struct {
    IntensityType   type;
    UnitType        unit;
    uint8_t         alert;
    union {
        uint32_t    u32_min;
        float       f32_min;
    };
    union {
        uint32_t    u32_max;
        float       f32_max;
    };
} IntensityTarget;

typedef struct {
    DurationType    type;
    UnitType        unit;
    union {
        uint32_t    u32_value;
        float       f32_value;
    };
} WorkoutDuration;

/*
 * Workout private data
 */
typedef struct {
    WorkoutPipelineType pp_type;
    IntensityTarget     target;
    WorkoutDuration     duration;
} WorkoutData;

/*
 * Group private data
 */
typedef struct {
    WorkoutPipelineType pp_type;
    WorkoutDuration     duration;
} WorkoutGroupData;


#ifdef __cplusplus
}
#endif
#endif /* WORKOUT_DATA_H_ */
