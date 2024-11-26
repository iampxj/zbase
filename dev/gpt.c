/*
 * Copyright 2024 wtcat
 */

#define pr_fmt(fmt) "<gpt>: "fmt 

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "basework/errno.h"
#include "basework/assert.h"
#include "basework/log.h"
#include "basework/malloc.h"
#include "basework/lib/string.h"
#include "basework/thirdparty/cJSON/cJSON.h"

#include "basework/dev/gpt.h"

#define field_size(type, filed) sizeof(((type *)0)->filed)

struct gp_table {
    char     version[12];
    uint32_t count;
    struct gpt_entry gps[];
};

static struct gp_table *gp_table;
static gpt_updated_cb gp_notify;

static const char *
get_string(cJSON *js, const char *s) {
    cJSON *obj = cJSON_GetObjectItemCaseSensitive(js, s);
    if (obj != NULL)
        return cJSON_GetStringValue(obj);
    return NULL;
}

static int 
get_value(cJSON *js, const char *s, int base, uint32_t *pval) {
    const char *sv = get_string(js, s);
    if (sv != NULL) {
        *pval = (uint32_t)strtoul(sv, NULL, base);
        return 0;
    }
    return -1;
}

int gpt_register_update_cb(gpt_updated_cb cb) {
    if (cb) {
        gp_notify = cb;
        return 0;
    }
    return -EINVAL;
}

const struct gpt_entry*
gpt_find(const char *name) {
    rte_assert(gp_table != NULL);
    if (name == NULL)
        return NULL;
    
    for (size_t i = 0; i < gp_table->count; i++) {
        if (!strcmp(name, gp_table->gps[i].name))
            return gp_table->gps + i;
    }
    return NULL;
}

void gpt_destroy(void) {
    if (gp_table) {
        general_free(gp_table);
        gp_table = NULL;
    }
}

void gpt_dump(void) {
    struct gp_table *gpt = gp_table;
    if (gpt == NULL)
        return;

    const char *devname = "device";
    const char *name = "name";
    size_t pname_max = strlen(name);
    size_t dname_max = strlen(devname);

    for (size_t i = 0; i < gpt->count; i++) {
        if (strlen(gpt->gps[i].name) > pname_max)
            pname_max = strlen(gpt->gps[i].name);

        if (strlen(gpt->gps[i].parent) > dname_max)
            dname_max = strlen(gpt->gps[i].parent);
    }

    pr_notice(
        "\n\n*********** Global Parition Table (version: %s) **************\n", 
        gpt->version
    );
    pr_notice("| %-*.*s | %-*.*s |   offset   |    length  |\n", 
        pname_max, 
        field_size(struct gpt_entry, name), 
        name, 
        dname_max,
        field_size(struct gpt_entry, parent),
        devname
    );

    for (size_t i = 0; i < gpt->count; i++) {
        pr_notice("| %-*.*s | %-*.*s | 0x%08lx | 0x%08x |\n", 
            pname_max, 
            field_size(struct gpt_entry, name), 
            gpt->gps[i].name, 
            dname_max,
            field_size(struct gpt_entry, parent),
            gpt->gps[i].parent, 
            gpt->gps[i].offset, 
            gpt->gps[i].size
        );
    }
    pr_notice("\n*******************************************************\n\n");
}

int gpt_load(const char *buffer) {
    rte_assert(buffer != NULL);
    struct gp_table *gp_new;
    const char *version;
    cJSON *root;
    cJSON *devs;
    cJSON *obj;
    int count;
    int err = 0;

    if (buffer == NULL)
        return -EINVAL;

    cJSON_Init();
    
    root = cJSON_Parse(buffer);
    if (root == NULL) {
        pr_err("Error*** parse failed(%s)\n", cJSON_GetErrorPtr());
        err = -EINVAL;
        goto _failed;
    }

    devs = cJSON_GetObjectItemCaseSensitive(root, "devices");
    if (devs == NULL) {
        pr_err("Not found node \"devices\"\n");
        err = -ENODATA;
        goto _failed;
    }

    version = get_string(root, "version");
    if (version == NULL) {
        pr_err("Not found node \"devices\"\n");
        err = -ENODATA;
        goto _failed;
    }

    /*
     * Calculate the parition counts and check key-nodes
     */
    count = 0;
    cJSON_ArrayForEach(obj, devs) {
        if (!get_string(obj, "name")) {
            pr_err("Not found node \"name\"\n");
            err = -ENODATA;
            goto _failed;
        }

        if (!get_string(obj, "storage")) {
            pr_err("Not found node \"storage\"\n");
            err = -ENODATA;
            goto _failed;
        }

        if (!get_string(obj, "capacity")) {
            pr_err("Not found node \"capacity\"\n");
            err = -ENODATA;
            goto _failed;
        }

        cJSON *pte = cJSON_GetObjectItemCaseSensitive(obj, "partitions");
        if (pte == NULL) {
            pr_err("Not found node \"partitions\"\n");
            err = -ENODATA;
            goto _failed;
        }
        count += cJSON_GetArraySize(pte);
    }

    if (count == 0) {
        pr_err("Invalid parition count(%d)\n", count);
        err = -EINVAL;
        goto _failed;
    }

    gp_new = general_calloc(1, sizeof(*gp_new) + 
        count * sizeof(struct gpt_entry));
    if (gp_new == NULL) {
        pr_err("No more memory\n");
        err = -ENOMEM;
        goto _failed;
    }

    /*
     * Parsing all partitions
     */
    cJSON_ArrayForEach(obj, devs) {
        cJSON *pte = cJSON_GetObjectItemCaseSensitive(obj, "partitions");
        const char *storage;
        uint32_t capacity = 0;
        int icount;
        cJSON *pte_obj;

        icount = gp_new->count;
        storage = get_string(obj, "storage");
        get_value(obj, "capacity", 16, &capacity);

        cJSON_ArrayForEach(pte_obj, pte) {
            const char *label = get_string(pte_obj, "label");
            const char *parent = get_string(obj, "name");
            uint32_t idx = gp_new->count;

            if (label == NULL) {
                pr_err("Not found node(\"label\") parent(%s)\n", parent);
                err = -ENODATA;
                goto _free;
            }

            if (get_value(pte_obj, "offset", 16, 
                &gp_new->gps[idx].offset) < 0) {
                pr_err("Not found node(\"offset\") parent(%s)\n", parent);
                err = -ENODATA;
                goto _free;
            }
            
            if (get_value(pte_obj, "size", 16, 
                &gp_new->gps[idx].size) < 0) {
                pr_err("Not found node(\"size\") parent(%s)\n", parent);
                err = -ENODATA;
                goto _free;
            }

            strlcpy(gp_new->gps[idx].name, label, 
                field_size(struct gpt_entry, name));

            strlcpy(gp_new->gps[idx].parent, storage, 
                field_size(struct gpt_entry, parent));

            gp_new->count = idx + 1;
        }

        /*
         * Check partition validity
         */
        for (int i = (int)gp_new->count - 1; i >= icount; i--) {
            const struct gpt_entry *gp = &gp_new->gps[i];
            if (gp->offset + gp->size > capacity) {
                pr_err("Partition(%s) paramter is invalid\n", gp->name);
                goto _free;
            }

            if (i > icount) {
                const struct gpt_entry *gp_next = &gp_new->gps[i - 1];
                if (gp_next->offset + gp_next->size > gp->offset) {
                    pr_err("Partition(%s) paramter is invalid\n", gp_next->name);
                    goto _free; 
                }
            }
        }
    }

    strlcpy(gp_new->version, version, 
        field_size(struct gp_table, version));
    gpt_destroy();
    gp_table = gp_new;
    cJSON_Delete(root);
    if (gp_notify)
        gp_notify();
        
    return 0;

_free:
    general_free(gp_new);
_failed:
    if (root)
        cJSON_Delete(root);
    return err;
}

int gpt_signature(
    int (*signature)(const void *buf, uint32_t size, void *ctx), 
    void *ctx) {
    const struct gp_table *gpt = gp_table;

    if (signature == NULL)
        return -EINVAL;

    if (!gpt || !gpt->count)
        return -EINVAL;

    return signature(gp_table, 
        sizeof(*gpt) + gpt->count * sizeof(struct gpt_entry), ctx);
}
