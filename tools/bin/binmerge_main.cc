/*
 * Copyright 2023 wtcat
 */
#include "tools.h"

int main(int argc, char *argv[]) {
    tools::cc_file_packge bpkg;

    //char *argv_v[] = {"binmerge.elf", "-d", "bins", "-o", "ota_full.ota"};
    //int argc_v = 5;
    if (!bpkg.parse(argc, argv))
        return -1;
    bpkg.merge_bin();

    return 0;
}