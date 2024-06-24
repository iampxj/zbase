/*
 * Copyright 2024 wtcat
 */

#ifdef CONFIG_HEADER_FILE
#include CONFIG_HEADER_FILE
#endif

#include <errno.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "basework/thirdparty/cJSON/cJSON.h"
#include "basework/misc/workout_core.h"
#include "basework/misc/workout_data.h"
#include "basework/log.h"

#define JSTR_MATCH(js, key) \
    for (char *__tmp_s = get_string(js, key); \
        __tmp_s; \
        __tmp_s = NULL)

#define JSTR_CASE(str, _exe) \
    if (strcmp(__tmp_s, str) == 0) { \
        _exe;                                \
        break;                               \
    }

static inline bool is_group(cJSON *js) {
    return cJSON_HasObjectItem(js, "Steps");
}

static char *get_string(cJSON *js, const char *s) {
    cJSON *obj = cJSON_GetObjectItemCaseSensitive(js, s);
    if (obj != NULL)
        return cJSON_GetStringValue(obj);
    return NULL;
}

static int get_int(cJSON *js, const char *s) {
    char *sv = get_string(js, s);
    if (sv != NULL)
        return strtol(sv, NULL, 10);
    return -1;
}

static int get_float(cJSON *js, const char *s) {
    char *sv = get_string(js, s);
    if (sv != NULL)
        return strtof(sv, NULL);
    return -1.0f;
}

static int get_steptype(cJSON *obj) {
    int type = K_WORKOUT_PP_OTHER;

    JSTR_MATCH(obj, "Step_type") {
        JSTR_CASE("Warm_up",     type = K_WORKOUT_PP_WARM_UP)
        JSTR_CASE("Run",         type = K_WORKOUT_PP_WORK)
        JSTR_CASE("Recovery",    type = K_WORKOUT_PP_RECOVERY)
        JSTR_CASE("Cool_down",   type = K_WORKOUT_PP_COOLDOWN)
    }
    return type;
}

/*
 * Run workout parse
 */
static int parse_duration(cJSON *js, WorkoutDuration *dur) {
    cJSON *obj;

    obj = cJSON_GetObjectItemCaseSensitive(js, "Duration");
    if (obj == NULL)
        return -ENODATA;
    
    /*
     * Parse "type" for duration
     */
    JSTR_MATCH(obj, "Type") {
        JSTR_CASE("Distance",     dur->type = K_DURATION_DISTANCE)
        JSTR_CASE("Time",         dur->type = K_DURATION_TIME)
        JSTR_CASE("Manually_lap", dur->type = K_DURATION_MANUALLY_LAP)
        JSTR_CASE("Auto_lap",     dur->type = K_DURATION_AUTO_LAP)
        JSTR_CASE("Heart_rate",   dur->type = K_DURATION_HEARTRATE)
    }

    /*
     * Parse "unit" for duration
     */
    JSTR_MATCH(obj, "Unit") {
        JSTR_CASE("Minutes",      dur->unit = K_UNIT_MINUTE)
        JSTR_CASE("Second",       dur->unit = K_UNIT_SECOND)
        JSTR_CASE("Kcal",         dur->unit = K_UNIT_KCAL)
        JSTR_CASE("Repetition",   dur->unit = K_UNIT_REPETITION)
    }

    /*
     * Parse "value" for duration
     */
    if (dur->type == K_DURATION_DISTANCE)
        dur->f32_value = get_float(obj, "Value");
    else
        dur->u32_value = (uint32_t)get_int(obj, "Value");

    return 0;
}

static int parse_intensity_target(cJSON *js, IntensityTarget *target) {
    cJSON *obj;

    obj = cJSON_GetObjectItemCaseSensitive(js, "Intensity_target");
    if (obj == NULL)
        return -ENODATA;

    /*
     * Parse "type" for intensity target
     */
    JSTR_MATCH(obj, "Type") {
        JSTR_CASE("Open",       target->type = K_INTENSITY_OPEN)
        JSTR_CASE("Cadence",    target->type = K_INTENSITY_CADENCE)
        JSTR_CASE("Heart_rate", target->type = K_INTENSITY_HEARTRATE)
    }

    /*
     * Parse "alert-switch" for intensity target
     */
    JSTR_MATCH(obj, "Alert_switch") {
        JSTR_CASE("on",         target->alert = 1)
        JSTR_CASE("off",        target->alert = 0)
    }

    if (target->type > K_INTENSITY_OPEN) {
        /*
        * Parse "unit" for duration
        */
        JSTR_MATCH(obj, "Unit") {
            JSTR_CASE("spm",              target->unit = K_UNIT_SPM)
            JSTR_CASE("bpm_reserve_rate", target->unit = K_UNIT_BPM_RESERVE_RATE)
        }

        if (target->unit == K_UNIT_BPM_RESERVE_RATE) {
            target->u32_min = get_int(obj, "Range_Lower_limit");
            target->u32_max = get_int(obj, "Range_upper_limit");
        } else {
            target->f32_min = get_float(obj, "Range_Lower_limit");
            target->f32_max = get_float(obj, "Range_upper_limit");
        }
    }

    return 0;
}

static int parse_workout_private(cJSON *js, WorkoutData *priv) {
    int err;

    err = parse_duration(js, &priv->duration);
    if (err == 0)
        err = parse_intensity_target(js, &priv->target);

    priv->pp_type = (WorkoutPipelineType)get_steptype(js);
    return err;
}


static int parse_domain_private(cJSON *js, WorkoutDomain *priv) {
    /*
     * Parse "ID"
     */
    priv->id = get_int(js, "Id");
    
    /*
     * Parse "type"
     */
    JSTR_MATCH(js, "Type") {
        JSTR_CASE("Run",        priv->type = K_WORKOUT_RUN)
        //TODO:
    }

    /*
     * Parse "type"
     */
    JSTR_MATCH(js, "Category") {
        JSTR_CASE("Easy_run",   priv->category = K_WORKOUT_CATEGORY_EASYRUN)
        //TODO:
    }

    return 0;
}

static int create_workout(cJSON *obj, struct workout_class *parent) {
    struct workout *workout;

    workout = workout_create(parent, get_string(obj, "Name"),
            get_string(obj, "Notes"), sizeof(WorkoutData));
    if (workout == NULL) {
        pr_err("create workout failed!\n");
        return -ENOMEM;
    }

    return parse_workout_private(obj, workout_private(workout));
}

static struct workout_group *create_group(cJSON *obj, struct workout_domain *domain) {
    struct workout_group *group;
    WorkoutGroupData *priv;

    group = group_workout_create(&domain->super, sizeof(WorkoutGroupData));
    if (group == NULL) {
        pr_err("create workout group failed!\n");
        return NULL;
    }

    priv = workoutclass_private(&group->super);
    if (parse_duration(obj, &priv->duration))
        return NULL;
    
    group->super.name = "Repeats";
    group->loop_count = (int8_t)priv->duration.u32_value;
    priv->pp_type = K_WORKOUT_PP_REPEATS;

    return group;
}

int workout_parse(const char *content, size_t size, struct workout_domain **p) {
    struct workout_domain *domain;
    struct workout_group *group;
    const char *title;
    const char *notes;
    cJSON *array;
    cJSON *obj, *child;
    cJSON *js;

    if (content == NULL || size == 0)
        return -EINVAL;

    js = cJSON_ParseWithLength(content, size);
    if (js == NULL)
        goto _parse_err;

    title = get_string(js, "Title");
    if (title == NULL)
        goto _freejs;

    notes = get_string(js, "Notes");
    if (notes == NULL)
        goto _freejs;

    /*
     * Create workout domain
     */
    domain = domain_workout_create(title, notes, sizeof(WorkoutDomain));
    if (domain == NULL) {
        pr_err("create workout domain failed!\n");
        goto _freejs;
    }

    if (parse_domain_private(js, workoutclass_private(&domain->super))) {
        pr_err("parse domain data failed\n");
        goto _free;
    }

    /*
     * Parse structure field
     */
    array = cJSON_GetObjectItemCaseSensitive(js, "Structure");
    if (array == NULL)
        goto _free;

    cJSON_ArrayForEach(obj, array) {
        if (!is_group(obj)) {
            if (create_workout(obj, &domain->super)) {
                pr_err("create workout failed!\n");
                goto _free;
            }
         
        } else {
            /*
             * Create workout group
             */
            group = create_group(obj, domain);
            if (group == NULL) {
                pr_err("create workout group failed!\n");
                goto _free;
            }

            /*
             * Parse children
             */
            array = cJSON_GetObjectItemCaseSensitive(obj, "Steps");
            if (array == NULL)
                goto _free;

            cJSON_ArrayForEach(child, array) {
                if (create_workout(child, &group->super)) {
                    pr_err("create workout failed!\n");
                    goto _free;
                }
            }
        }
    }

    cJSON_Delete(js);
    if (p != NULL)
        *p = domain;

    return 0;

_free:
    domain_workout_destroy(domain);
_freejs:
    cJSON_Delete(js);
_parse_err:
    pr_err("parse error: %s\n", cJSON_GetErrorPtr());
    return -ENOENT;
}
