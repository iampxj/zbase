/*
 * Copyright 2023 wtcat  
 */
#include <stdio.h>
#include <getopt.h>

#include <fstream>
#include <string>

#define kFillBufferSize 8192

#define MIN(a, b) ((a) < (b))? (a): (b)

/*
 * Clone flags
 */
#define CLONE_APPEND     0x01

static int clone_binary(const std::string &dst, const std::string &src, 
    size_t newsize, unsigned int cloneflags) {
    std::fstream fp_src(src, std::ios::in | std::ios::binary);
    if (!fp_src.is_open()) {
        printf("Error***: file(%s) is not exist", src.c_str());
        return -EINVAL;
    }
    size_t fsize_src;
    fp_src.seekg(0, std::ios::end);
    fsize_src = (size_t)fp_src.tellg();
    fp_src.seekg(0, std::ios::beg);

    /* Convert flags */
    unsigned int oflag = std::ios::out | std::ios::binary;
    bool fill;
    if (!(cloneflags & CLONE_APPEND)) {
        oflag |= std::ios::trunc;
        fill = true;
    } else {
        oflag |= std::ios::app;
        fill = false;
        newsize = UINT32_MAX;
    }

    /* Open/New target file */
    std::fstream fp_dst(dst, oflag);
    if (!fp_dst.is_open()) {
        printf("Error***: open %s error\n", src.c_str());
        return -EINVAL;
    }

    /* Copy file */
    size_t cpsize = MIN(fsize_src, newsize);
    size_t offset = cpsize;
    std::unique_ptr<char[]> p = std::make_unique<char[]>(kFillBufferSize);
    while (cpsize > 0) {
        size_t rdsize = MIN(cpsize, kFillBufferSize);
        fp_src.read(p.get(), rdsize);
        if (fp_src.fail()) {
            printf("Error***: File i/o error\n");
            return -EIO;
        }
        fp_dst.write(p.get(), rdsize);
        cpsize -= rdsize;
    }

    /* Fill file */
    if (fill) {
        newsize -= offset;
        if (newsize > 0) {
            size_t fillsize = MIN(newsize, kFillBufferSize);
            memset(p.get(), 0, fillsize);
            do {
                fp_dst.write(p.get(), fillsize);
                newsize -= fillsize;
                fillsize = MIN(newsize, kFillBufferSize);
            } while (newsize > 0);
        }
    }

    fp_dst.close();
    return 0;
}

int parse_args(int argc, char *argv[], std::string *dstfile, 
    std::string *srcfile, size_t *size, unsigned int *flags) {
    int mark = 0;
    int ch;
    
    static const char usage[] = {
        "binclone -o [output] -i [input] [-s size] [-a]"
    };
    while ((ch = getopt(argc, argv, "o:i:s:a")) != -1) {
        switch (ch) {
        case 'o':
            dstfile->append(optarg);
            mark |= 0x1;
            break;
        case 'i':
            srcfile->append(optarg);
            mark |= 0x2;
            break;
        case 'a':
            *flags = CLONE_APPEND;
            mark |= 0x4;
            break;
        case 's':
            *size = strtoul(optarg, NULL, 16);
            mark |= 0x8;
            break;
        default:
            printf(usage);
            break;
        }
    }
    if ((mark & 0x3) != 0x3 ||
        (mark & 0xc) == 0xc) {
        printf(usage);
        return -EINVAL;
    }
    if (!(mark & 0x4) && *size == 0) {
        puts("Error***: Not found size parameter! (-s)\n");
        return -EINVAL;
    }
    return 0;
}

int main(int argc, char **argv) {
    std::string src, dst;
    size_t size = 0;
    unsigned int flags = 0;
    int err;
#if 0
    char *argvv[] = {"binclone", 
        "-i", "zephyr.bin",
        "-o", "newzephyr.bin",
        "-s", "0x200000",
        //"-a"
    };
    int argcc = sizeof(argvv)/sizeof(argvv[0]);

    if (parse_args(argcc, argvv, &dst, &src, &size, &flags))
        return -EINVAL;
#else
    if (parse_args(argc, argv, &dst, &src, &size, &flags))
        return -EINVAL;
#endif
    err = clone_binary(dst, src, size, flags);
    if (!err)
        printf("Clone file(%s) success\n", dst.c_str());
    else
        printf("Error***: Clone failed!(%d)\n", err);
    return err;
}