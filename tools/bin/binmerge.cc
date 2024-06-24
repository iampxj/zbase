/*
 * Copyright 2023 wtcat
 */
#include <string.h>
#include <getopt.h>

#include <iostream>
#include <vector>
#include <fstream>
#include <memory>
#include <filesystem>

#include "basework/utils/binmerge.h"
#include "tools.h"


namespace tools {

void cc_file_packge::show_usage() const {
    printf(
        "binmerge -o [output] -d [path]\n"
        "\t-o    output file name\n"
        "\t-d    directory of binary file\n"
    );
}

bool cc_file_packge::parse(int argc, char *argv[]) {
    int ch;

    if (argc == 1) {
        show_usage();
        return false;
    }
    while ((ch = getopt(argc, argv, "d:o:")) != -1) {
        switch (ch) {
        case 'd':
            dir_ = optarg;
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

bool cc_file_packge::get_flist(const std::string& filter) {
    std::filesystem::directory_iterator dir(dir_);
    for (auto &iter : dir) {
        if (iter.is_directory())
            continue;
        if (iter.is_regular_file() && 
            iter.path().extension() == filter) {
            flist_.push_back(iter.path().string());
        }
    }
    return !flist_.empty();
}

bool cc_file_packge::merge_bin(void) {
    struct file_header *fh;
    struct file_node *fn;
    std::fstream fd;
    size_t hdrsize;

    // Load file list
    if (!get_flist(".bin")) {
        printf("Not found binary file\n");
        return false;
    }
    if (ofile_.empty()) 
        ofile_ = "merged.bin";
    
    // Allocate memory for file header
    hdrsize = sizeof(struct file_header) + flist_.size() * sizeof(struct file_node);
    std::unique_ptr<char[]> ptr = std::make_unique<char[]>(hdrsize);
    fh = (struct file_header *)ptr.get();
    fh->magic = FILE_HMAGIC;
    fh->nums = flist_.size();
    fn = fh->headers;

    // Create output file and reserve file header
    fd.open(ofile_, std::ios::binary | std::ios::out | std::ios::trunc);
    if (!fd.is_open()) {
        printf("open file(%s) error\n", ofile_.c_str());
        return false;
    }
    fd.seekp(hdrsize , std::ios::beg);

    size_t f_offset = hdrsize;
    uint32_t crc32 = 0;
    for (auto & iter : flist_) {
        std::fstream frd(iter.c_str(), std::ios::in | std::ios::binary);
        if (!frd.is_open()) {
            printf("Open file(%s) failed\n", iter.c_str());
            return false;
        }

        // Load file to buffer
        frd.seekg(0, std::ios::end);
        size_t fsize = frd.tellg();
        std::unique_ptr<char[]> buffer = std::make_unique<char[]>(fsize);
        frd.seekg(0, std::ios::beg);
        frd.read(buffer.get(), fsize);
        frd.close();
        if (frd.fail()) {
            printf("Read file failed\n");
            fd.close();
            return false;
        }

        // Fill file header information
        std::filesystem::path fname(iter);
        auto fstr = fname.filename().string();

        filter(fstr, buffer.get(), fsize);

        size_t namelen = (fstr.size() >= MAX_NAMELEN)? MAX_NAMELEN - 1: iter.size();
        strncpy(fn->f_name, fstr.c_str(), namelen);
        fn->f_name[namelen] = '\0';
        fn->f_offset = f_offset;
        fn->f_size = fsize;

        // Calculate CRC
        crc32 = crc32_update(crc32, (const uint8_t *)buffer.get(), fsize);

        // Write buffer to output file
        fd.write(buffer.get(), fsize);

        fn++;
        f_offset += fsize;
    }

    // Write file header
    fh->size = f_offset + hdrsize;
    fh->crc = crc32;
    fd.seekp(0 , std::ios::beg);
    fd.write((const char *)fh, hdrsize);
    fd.close();
    return true;
}

} //namespace tools

