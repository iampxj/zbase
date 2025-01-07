/*
 * Copyright 2024 wtcat
 */

#define pr_fmt(fmt) "<fw_param>: "fmt

#include <errno.h>
#include "basework/lib/string.h"
#include "basework/lib/libenv.h"
#include "basework/thirdparty/cJSON/cJSON.h"
#include "basework/log.h"

static const char *env_keys[] = {
    "AppLink",
    "AppName",
    "BtName",
    "FwVersion",
    "TargetDevID",
    "LanguageDefault",
    NULL
};

static const char *regulatory_keys[] = {
    "Status",
    "FCC",
    "ICC",
    NULL
};

static const char *logo_keys[] = {
    "X",
    "Y",
    "Type",
    "AnimTime",
    "AnimTimes",
    NULL
};

static const char *
get_string(cJSON *js, const char *s) {
    cJSON *obj = cJSON_GetObjectItemCaseSensitive(js, s);
    if (obj != NULL)
        return cJSON_GetStringValue(obj);
    return NULL;
}

static void load_params(cJSON *obj, const char *keys[], const char *prefix) {
    for (int i = 0; keys[i] != NULL; i++) {
        const char *ps = get_string(obj, keys[i]);
        if (ps) {
            if (prefix) {
                char keyname[128];
                size_t offset = strlcpy(keyname, prefix, sizeof(keyname));
                strlcpy(&keyname[offset], keys[i], sizeof(keyname) - offset);
                pr_notice("%s -> %s\n", keyname, ps);
                rte_setenv(keyname, ps, 1);
            } else {
                pr_notice("%s -> %s\n", keys[i], ps);
                rte_setenv(keys[i], ps, 1);
            }
        }
    }
}

static void load_language_params(cJSON *env) {
    char buffer[512] = {0};
    int offset = 0;
    const char *p;

    cJSON *lang = cJSON_GetObjectItemCaseSensitive(env, "LanguageActive");
    if (env == NULL) {
        pr_err("Not found node \"LanguageActive\"\n");
        return;
    }

    int size = cJSON_GetArraySize(lang);
    for (int i = 0; i < size; i++) {
        cJSON *item = cJSON_GetArrayItem(lang, i);
        p = cJSON_GetStringValue(item);
        if (p) {
            offset += strlcat(buffer, p, sizeof(buffer) - offset);
            offset += strlcat(buffer, ";", sizeof(buffer) - offset);
        }
    }
    rte_setenv("LanguageActive", buffer, 1);
}

int fw_load_pararm(const char *buffer) {
    cJSON *root, *env;
    cJSON *logo, *regulatory;
    int err = -EINVAL;

    cJSON_Init();
    root = cJSON_Parse(buffer);
    if (root == NULL) {
        pr_err("Error*** parse failed(%s)\n", cJSON_GetErrorPtr());
        err = -EINVAL;
        goto _failed;
    }

    env = cJSON_GetObjectItemCaseSensitive(root, "Env");
    if (env == NULL) {
        pr_err("Not found node \"Env\"\n");
        err = -ENODATA;
        goto _failed;
    }
    load_params(env, env_keys, NULL);
    load_language_params(env);

    regulatory = cJSON_GetObjectItemCaseSensitive(env, "Regulatory");
    if (regulatory == NULL) {
        pr_err("Not found node \"Regulatory\"\n");
        err = -ENODATA;
        goto _failed;
    }
    load_params(regulatory, regulatory_keys, "Regulatory");

    logo = cJSON_GetObjectItemCaseSensitive(env, "Logo");
    if (logo == NULL) {
        pr_err("Not found node \"Logo\"\n");
        err = -ENODATA;
        goto _failed;
    }
    load_params(logo, logo_keys, "Logo");

_failed:
    cJSON_Delete(root);
    return err;
}
