/*
 * Copyright 2023 wtcat
 */
#include "basework/utils/binmerge.h"
#include "tools/tools.h"

#include <getopt.h>
#include <stdio.h>
#include <string.h>

#include <memory>
#include <fstream>

namespace tools {

class cc_bin_wrapper {
public:
    cc_bin_wrapper() = default;
    ~cc_bin_wrapper() = default;

    bool make_fbin(const std::string &in, const std::string &out);
};

bool cc_bin_wrapper::make_fbin(const std::string &in, const std::string &out) {
    /* Read binary file */
    std::fstream fs(in, std::ios::in | std::ios::binary);
    if (!fs.is_open()) {
        printf("Error***: open %s failed\n", in.c_str());
        return false;
    }
    fs.seekg(0, std::ios::end);
    size_t fsize = (size_t)fs.tellg();
    size_t nfsize = fsize + sizeof(struct bin_header);
    std::unique_ptr<char[]> fp = std::make_unique<char[]>(nfsize);
    struct bin_header *hbin = (struct bin_header *)fp.get();
    memset(hbin, 0, sizeof(*hbin));
    
    fs.seekg(0, std::ios::beg);
    fs.read(hbin->data, fsize);
    if (fs.bad()) {
        printf("Error***: read file failed\n");
        return false;
    }
    fs.close();

    /* Fill file header */
    hbin->magic = FILE_HMAGIC;
    hbin->size = fsize;
    hbin->crc = crc32_update(0, (const uint8_t *)hbin->data, fsize);

    /* Write in new file */
    std::fstream fout(out, std::ios::out | std::ios::binary | std::ios::trunc);
    if (!fout.is_open()) {
        printf("Error***: open %s failed\n", out.c_str());
        return false;
    }
    fout.write((const char *)hbin, nfsize);
    fout.close();
    return true;
    
}
} //namespace tools

static void show_usage(void) {
    printf("\nmkbin [-i] input [-o] output\n");
}

int main(int argc, char *argv[]) {
    const char *fin = NULL;
    const char *fout = NULL;
    int ch;

    if (argc != 5) {
        show_usage();
        return -1;
    }
    while ((ch = getopt(argc, argv, "i:o:")) != -1) {
        switch (ch) {
        case 'i':
            fin = optarg;
            break;
        case 'o':
            fout = optarg;
            break;
        default:
            show_usage();
            return -2;
        }
    }

    if (!fin || !fout)
        return -1;

    tools::cc_bin_wrapper bin;
    bin.make_fbin(fin, fout);
    return 0;
}