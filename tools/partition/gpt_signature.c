/*
 * Copyright 2024 wtcat
 */

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>

#include "basework/lib/string.h"
#include "basework/lib/crc.h"
#include "basework/dev/gpt.h"
#include "basework/log.h"
#include "basework/utils/binmerge.h"
#include "basework/thirdparty/cJSON/cJSON.h"

#include "tinycrypt/sha256.h"


#define logerr(fmt, ...) printf("***Error: " fmt, ##__VA_ARGS__)

static struct printer log_printer;

#define COPY_TEXT(fmt, ...) \
    c_textlen += snprintf(&c_text[c_textlen], \
    sizeof(c_text) - c_textlen, fmt, ##__VA_ARGS__)


static char   c_text[1024];
static size_t c_textlen;

static int signature(const void* buf, uint32_t size, void* ctx) {
    struct tc_sha256_state_struct state;
    uint8_t *shakey = (uint8_t *)ctx;

    (void) tc_sha256_init(&state);
    (void) tc_sha256_update(&state, (const uint8_t*)buf, size);
    (void) tc_sha256_final(shakey, &state);

    return 0;
}

static void generate_code(const uint8_t *key, size_t len, const char *outfile) {
    /*
     * Code header
     */
    COPY_TEXT(
        "/*\n"
        " * Sha256 for partition\n"
        " */\n"
        "#ifndef gpt_sha256_h__\n"
        "#define gpt_sha256_h__\n\n"
        "#include <stdint.h>\n"
        "#include <stdbool.h>\n"
        "#include <string.h>\n\n"
        "static const unsigned char gpt__sha256[] = {"
    );

    /*
     * Code body
     */
    for (size_t i = 0; i < len; i++) {
        if (i % 8 == 0)
            COPY_TEXT("\n\t");
        COPY_TEXT("0x%02x, ", key[i]);
    }

    /*
     * Code foot
     */
    COPY_TEXT(
        "\n};\n\n"
        "static inline bool gpt_signagure_verify(const uint8_t *key) {\n"
        "\treturn !memcmp(key, gpt__sha256, 32);\n"
        "}\n\n"
        "#endif /* gpt_sha256_h__ */\n"
    );

    FILE *ofp = fopen(outfile, "w");
    if (ofp) {
        fwrite(c_text, c_textlen, 1, ofp);
        fclose(ofp);
    }
}

int main(int argc, char* argv[]) {
    struct bin_header* bin;
    const char *file;
    char header[256];
    char binfile[256];
    FILE* fp;
    size_t olen;
    long len;
    int err = 0;

#ifdef DEBUG
    argc = 2;
    file = "partition.json";
#else
    file = argv[1];
#endif
    if (argc < 2 || argc > 3) {
        printf("gpt_signature [inputFile] [OutputPath]\n\n");
        return -EINVAL;
    }

    if (argc == 3) {
        size_t cp_size;
        cp_size = strlcpy(header, argv[2], sizeof(header));
        strlcpy(header + cp_size, "/gpt_signature.h", sizeof(header) - cp_size);

        cp_size = strlcpy(binfile, argv[2], sizeof(binfile));
        strlcpy(binfile + cp_size, "/gpt.bin", sizeof(binfile) - cp_size);
    } else {
        strlcpy(header, "gpt_signature.h", sizeof(header));
        strlcpy(binfile, "gpt.bin", sizeof(binfile));
    }

    printf_format_init(&log_printer);
    pr_log_init(&log_printer);
    cJSON_Init();

    fp = fopen(file, "r");
    if (fp == NULL)
        return -EINVAL;

    fseek(fp, 0, SEEK_END);
    len = ftell(fp);
    if (len <= 0) {
        logerr("file size is 0\n");
        err = -ENODATA;
        goto _exit;
    }
    if (len > 4096) {
        logerr("file is large than 4096!\n");
        err = -EINVAL;
        goto _exit;
    }

    olen = sizeof(*bin) + len + 1;
    bin = calloc(1, olen);
    if (bin == NULL) {
        err = -ENOMEM;
        goto _exit;
    }

    fseek(fp, 0, SEEK_SET);
    fread(bin->data, 1, (size_t)len, fp);

    if (!gpt_load(bin->data)) {
        gpt_dump();
        printf("\ncheck ok!\n");

        FILE* ofp = fopen(binfile, "wb");
        if (ofp) {
            bin->magic = FILE_HMAGIC;
            bin->size = strlen(bin->data);
            bin->crc = lib_crc32((uint8_t *)bin->data, bin->size);
            fwrite(bin, 1, olen, ofp);
            fclose(ofp);
        }

        /*
         * Generate partition signature  
         */
        uint8_t shakey[32] = { 0 };
        err = gpt_signature(signature, shakey);
        assert(err == 0);
        generate_code(shakey, sizeof(shakey), header);
    }

    gpt_destroy();
    free(bin);

_exit:
    fclose(fp);
    return err;
}
