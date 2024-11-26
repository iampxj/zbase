/*
 * Copyright 2022 wtcat
 */

#ifdef CONFIG_HEADER_FILE
#include CONFIG_HEADER_FILE
#endif

#include <errno.h>
#include <string.h>

#include "basework/generic.h"
#include "basework/dev/partition.h"
#include "basework/dev/partition_file.h"

#define BYTE(n) SFILE_SIZE(n)
#define KB(n) SFILE_SIZE(n * 1024)

#define PT_ENTRY(name, size) \
    PARTITION_ENTRY(name, NULL, -1, size)

/*
 * Partition configure table
 */
static const struct disk_partition partitions_usr_configure[] = {
#ifdef CONFIG_CUSTOM_PT_TABLE
    #include CONFIG_MINOR_PT_FILE
#else
    #include "config/minor_ptcfg_default.h"
#endif
    PARTITION_TERMINAL
};

/*
 * This partition can not be erased when the system reset factory settings
 */
static const struct disk_partition partitions_usr2_configure[] = {
#ifdef CONFIG_CUSTOM_PT_TABLE
    #include CONFIG_MINOR_PT2_FILE
#else
    #include "config/minor_ptcfg2_default.h"
#endif
    PARTITION_TERMINAL
};

enum config_items {
    USER_CFG_ITEMS  = rte_array_size(partitions_usr_configure),
    USER2_CFG_ITEMS = rte_array_size(partitions_usr2_configure),
};

int usr_partition_init(void) {
    static struct disk_partition user1_pt[USER_CFG_ITEMS];
    static struct disk_partition user2_pt[USER2_CFG_ITEMS];
    int err;

    memcpy(user1_pt, partitions_usr_configure, sizeof(user1_pt));
    memcpy(user2_pt, partitions_usr2_configure, sizeof(user2_pt));

    usr_sfile_init();
    err = logic_partitions_create("usrdata", user1_pt);
    if (!err)
        err = logic_partitions_create("usrdata2", user2_pt);
    return err;
}
