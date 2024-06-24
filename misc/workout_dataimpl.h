/*
 * Copyright 2024 wtcat
 */
#ifndef WORKOUT_DATAIMPL_H_
#define WORKOUT_DATAIMPL_H_

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#include "basework/container/queue.h"

#ifdef __cplusplus
extern "C"{
#endif

#define MAX_SPORTS 140

#if defined(__linux__)
# define SPFILE_ROOT "./"
#elif defined(_WIN32)
# define SPFILE_ROOT "d:\\"
#else
# define SPFILE_ROOT "/SPF:/"
#endif 

# define SPFILE_NAME(name) SPFILE_ROOT name

struct sport_record;

typedef struct {
    const char *code;
    uint16_t    count;
    uint16_t    offset;
} SportSortedItem;

typedef struct {
    const uint8_t    *types;
    uint16_t         typecnt;
    uint16_t         itemcnt;
    SportSortedItem  items[];
} SportSortedList;

typedef struct {
    uint8_t        hour;
    uint8_t        min;
    uint8_t        sec;
    uint8_t        pad[1];
} SportDuration;

typedef struct {
    uint8_t        min;
    uint8_t        sec;  
} SportPace;

typedef struct {
    float          coord_x;
    float          coord_y;
} SportCoordinate;

typedef struct {
    int            tm_sec;
    int            tm_min;
    int            tm_hour;
    int            tm_mday;
    int            tm_mon;
    int            tm_year;
} SportTime;

typedef struct {
    SportDuration  sp_duration; /* 00:00:00 - 99:59:00 */
    SportTime      sp_start_time;

    SportDuration  sp_target_duration; /* 00:00:00 - 23:59:00 */
    uint32_t       sp_target_calorie;  /* 0 - 5000 */
    uint32_t       sp_target_distance; /* 0 - 999.99km */

    uint8_t        sp_heartrate_average; /* 35 - 210 */
    uint8_t        sp_heartrate_max;
    uint8_t        sp_heartrate_range_[10];
    SportDuration  sp_heartrate_range[5];

    uint32_t       sp_distance; /* 0 - 999999m */
    uint32_t       sp_colarie_workout;
    uint32_t       sp_colaire_total;

    uint32_t       sp_steps;

    SportPace      sp_pace_average;
    SportPace      sp_pace_best;

    float          sp_speed_average;
    float          sp_speed_min;

    uint16_t       sp_steprate_average;
    uint16_t       sp_steprate_max;

    uint16_t       sp_stepwidth; /* 0 - 300cm */

    uint16_t       sp_oxygen_workout_level; /* x10 rang[0.0, 5.0] */
    uint16_t       sp_oxygen_breath_max; /* x10 rang[00.0, 60.0] */
    uint16_t       sp_cooldown_time; /* 0h0min - 99h0min */

} SportDataInstance;

#pragma pack(1)
typedef struct {
    float           speed;  /* 0 - 999.99km/h */
    SportPace       pace;
    uint16_t        steprate;  /* 0 - 300step/min */
    uint16_t        stepwidth; /* 0 - 300cm */
    uint8_t         heartrate[3]; /* 35 - 210 */
} SportRealtimeData3s;
#pragma pack()

typedef struct {
    char                name[52]; /* Sport name */
    char                gps[56];
    SportDataInstance   data;
    uint32_t            count; /* */
    uint32_t            hrlast_count; /* */
} SportDatabase;

typedef struct {
    /* Calculate target value for workout */
    void (*process)(struct sport_record *srd, SportDataInstance *inst);

    /* Error process handler */
    void (*perror)(struct sport_record *srd, int err);

    /* Initialize sport data */
    void (*init)(SportDataInstance *inst);

    /* Get coordinate data */
    int  (*get_coord)(SportCoordinate *coord);
    int  (*get_data3s)(SportRealtimeData3s *d3s, int second);

    /* Get time from rtc */
    int  (*get_time)(SportTime *tm);
} SportDataOperations;

typedef struct {
#define SPORT_DATNAME_LEN 32
    char            name[SPORT_DATNAME_LEN];
    uint32_t        chksum;
    char            buffer[];
} SportDataHeader;

struct sport_recdata {
    SportDataHeader *data;
    uint32_t         dirty;
};

#define SPORT_COMP_NONE 0x00
#define SPORT_COMP_LZ4  0x01

struct file_writer;
struct data_queue;

struct sport_record {
#define SPORT_SEM_STRUCT_SIZE 32
    TAILQ_HEAD(, file_writer) sync_list;
    const SportDataOperations *dops;
    void                      *fd;
    void                      *fd_gps;
    void                      *timer;
    struct data_queue         *main_queue;
    struct data_queue         *gps_queue;
    uint32_t                  timer3s_counter;
    uint32_t                  time_counter;
    uint32_t                  time_threshold;
    uint32_t                  period_ms;
    SportRealtimeData3s       data3s;
    uint32_t                  heartrate_sum;
    float                     speed_sum;
    uint32_t                  steprate_sum;
    int                       error;
    uint32_t                  should_stop;
#define SPORT_TIMER_PAUSE_F    0x01
#define SPORT_TIMER_CLOSE_F    0x02

    SportDatabase             db;
    uintptr_t                 sem[SPORT_SEM_STRUCT_SIZE/sizeof(uintptr_t)];
};

struct record_files {
#define MAX_SPORT_FILES 20
    char  name[MAX_SPORT_FILES][64];
    char *pname[MAX_SPORT_FILES];
    int   index;
};;


struct spdata_object;
#define WORKOUT_DATA_RECORD(_name, _file, _datasize, _reset_fn) \
    struct spdata_object _name##_obj = {         \
        .name   = #_name,                        \
        .path   = WORKOUT_PATH(_file),           \
        .osize  = _datasize,                     \
        .loaded = 0,                             \
        .load   = sport_general_load,            \
        .unload = sport_general_unload,          \
        .reset  = _reset_fn                      \
    };                                           \
    WORKOUT_SPDATA_REGISTER(_name##_obj)

int sport_general_load(struct spdata_object *o);
int sport_general_unload(struct spdata_object *o);

int sport_record_open(const char *name, uint32_t period, size_t qsize, 
    int comp, const SportDataOperations *dops, struct sport_record *srd);
int sport_record_active(struct sport_record *srd, bool active);
int sport_record_close(struct sport_record *srd, bool wait);

int spfile_constrain(const char *dir, int maxfiles);
int spfile_open(const char *name, void **pfd);
int spfile_close(void *fd);
int spfile_read(void *fd, void *buffer, size_t size);

int spmain_file_open(const char *path, uint32_t rdburst, void **pfp);
int spmain_file_close(void *fp);
int spmain_file_getdb(void *fp, SportDataInstance **pdb);
int spmain_file_walk(void *fp, bool restart,
    bool (*fread)(const SportRealtimeData3s *d3s, uint32_t nitem, void *param),
    void *param);

#ifdef __cplusplus
}
#endif
#endif /* WORKOUT_DATAIMPL_H_ */
