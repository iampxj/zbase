/*
 * Copyright 2023 wtcat
 */
#include <string.h>
#include <stdlib.h>
#include <getopt.h>

#include "tools.h"

struct sd_dir {
	const char fname[12];	//8+1+3
	int32_t offset;
	int32_t size;
	uint32_t reserved[2];
	uint32_t checksum;
};

namespace tools {

class cc_binpatch : public cc_file_packge {
public:
    bool parse(int argc, char *argv[]) override;
protected:
    void filter(const std::string &name, void *buffer, size_t size) override;
    void show_usage() const override;
private:
    uint32_t id_;
};

void cc_binpatch::filter(const std::string &name, void *buffer, size_t size)  {
    if (name == "res.bin" || name == "fonts.bin") {
        struct sd_dir *dir = (struct sd_dir *)buffer;
        if (!strcmp(dir->fname, "sdfs.bin")) {
            dir->reserved[0] = id_;
            printf("%s patch version(%d)\n", name.c_str(), id_);
        }
    }
}

void cc_binpatch::show_usage() const {
    printf(
        "binmerge -o [output] -d [path] -m [id]\n"
        "\t-o    output file name\n"
        "\t-d    directory of binary file\n"
        "\t-m    resource file version"
    );
}

bool cc_binpatch::parse(int argc, char *argv[]) {
    int ch;

    if (argc == 1) {
        cc_binpatch::show_usage();
        return false;
    }
    while ((ch = getopt(argc, argv, "m:d:o:")) != -1) {
        switch (ch) {
        case 'd':
            dir_ = optarg;
            break;
        case 'm':
            id_ = strtoul(optarg, NULL, 10);
            break;
        case 'o':
            ofile_ = optarg;
            break;
        default:
            show_usage();
            return false;
        }
    }

    return !dir_.empty() && !ofile_.empty();
}
}

int main(int argc, char *argv[]) {
    tools::cc_binpatch patch_file;
    tools::cc_file_packge &bpkg = patch_file;
    

    //char *argv_v[] = {"binmerge.elf", "-d", "bins", "-o", "ota_full.ota"};
    //int argc_v = 5;
    if (!bpkg.parse(argc, argv))
        return -1;
    bpkg.merge_bin();

    return 0;
}