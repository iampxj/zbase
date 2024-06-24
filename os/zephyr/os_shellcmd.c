/*
 * Copyright 2023 wtcat
 */

#ifdef CONFIG_HEADER_FILE
#include CONFIG_HEADER_FILE
#endif

#include <shell/shell.h>

#include "basework/dev/partition.h"
#include "basework/system.h"

static int __rte_notrace shell_filesystem_format(const struct shell *shell,
    size_t argc, char **argv) {
    (void) shell;
    (void) argv;
    if (argc > 1)
        return -EINVAL;

    const struct disk_partition *fpt = disk_partition_find("filesystem");
    if (fpt) {
        disk_partition_erase_all(fpt);
        printk("Successful\n");
        sys_shutdown(0, true);
        return 0;
    }
    return -EIO;
}

SHELL_STATIC_SUBCMD_SET_CREATE(cmd_list,
    SHELL_CMD(format, NULL, "filesystem format" , shell_filesystem_format),
    SHELL_SUBCMD_SET_END
);

SHELL_CMD_REGISTER(user, &cmd_list, "User shell commands", NULL);
