/*
 * Copyright 2024 wtcat
 */

#ifdef CONFIG_HEADER_FILE
#include CONFIG_HEADER_FILE
#endif

#define pr_fmt(fmt) "<installer>: "fmt
#define CONFIG_LOGLEVEL LOGLEVEL_DEBUG

#include <errno.h>
#include <assert.h>
#include <stdbool.h>
#include <string.h>
#include <sys/types.h>

#include "basework/generic.h"
#include "basework/utils/binmerge.h"
#include "basework/boot/boot.h"
#include "basework/dev/disk.h"
#include "basework/log.h"
#include "basework/minmax.h"

#define MEDIA_BLOCK_SIZE 4096

struct copy_device {
    struct disk_device *dst;
    struct disk_device *src;
    uint32_t            ioffset;
    void  (*notify)(const char *name, int percent);
};

struct copy_node {
    const char *name;
    uint32_t dst_offset;
    uint32_t src_offset;
    uint32_t size;
};

PACKAGE_HEADER(package_header, MAX_FWPACK_FILES);
static uint8_t global_buffer[8192];

static void fw_package_dump(const struct file_header *header, 
    const struct fwpkg_record *pi) {
    pr_dbg("*package current*: magic(0x%x) hcrc(0x%x) num(%d)\n",
        pi->magic, pi->hcrc, pi->count);
    if (pi->count < MAX_FWPACK_FILES) {
        for (uint32_t i = 0; i < pi->count; i++) {
            pr_dbg("\tname(%s) offset(0x%x) size(0x%x)\n", 
                pi->nodes[i].f_name,
                pi->nodes[i].f_offset,
                pi->nodes[i].f_size
            );
        }
    }

    pr_dbg("*package header*:magic(0x%x) crc(0x%x) size(0x%x) num(%d)\n",
        header->magic, header->crc, header->size, header->nums);
    if (header->nums < MAX_FWPACK_FILES) {
        for (uint32_t i = 0; i < header->nums; i++) {
            pr_dbg("\tname(%s) offset(0x%x) size(0x%x)\n", 
                header->headers[i].f_name,
                header->headers[i].f_offset,
                header->headers[i].f_size
            );
        }
    }
}

static void
copy_notify(struct copy_device *cdev, const char *name, uint32_t max, 
    uint32_t remain, uint32_t *old) {
    if (cdev->notify && *old != remain) {
        uint32_t percent = ((max - remain) * 100) / max;
        cdev->notify(name, percent);
        *old = remain;
    }
}

static uint32_t 
crc32_calc(uint32_t crc, const uint8_t *data, size_t len) {
	/* crc table generated from polynomial 0xedb88320 */
	static const uint32_t table[16] = {
		0x00000000U, 0x1db71064U, 0x3b6e20c8U, 0x26d930acU,
		0x76dc4190U, 0x6b6b51f4U, 0x4db26158U, 0x5005713cU,
		0xedb88320U, 0xf00f9344U, 0xd6d6a3e8U, 0xcb61b38cU,
		0x9b64c2b0U, 0x86d3d2d4U, 0xa00ae278U, 0xbdbdf21cU,
	};

	crc = ~crc;
	for (size_t i = 0; i < len; i++) {
		uint8_t byte = data[i];
		crc = (crc >> 4) ^ table[(crc ^ byte) & 0x0f];
		crc = (crc >> 4) ^ table[(crc ^ ((uint32_t)byte >> 4)) & 0x0f];
	}

	return ~crc;
}

static int 
file_checksum(uint32_t *pcrc, struct disk_device *dev, 
    uint32_t offset, size_t size) {
    uint32_t crc = *pcrc;
    int ret;

    while (size > 0) {
        size_t bytes = rte_min(size, sizeof(global_buffer));
        ret = disk_device_read(dev, global_buffer, bytes, offset);
        if (ret) 
            return ret;
        crc    = crc32_calc(crc, global_buffer, bytes);
        offset += bytes;
        size   -= bytes;
    }

    *pcrc = crc;
    return 0;
}

static int 
file_copy(struct copy_device *cdev, const char *name,
    uint32_t dofs, uint32_t sofs, size_t size) {
    size_t erase_size = RTE_ALIGN(size, MEDIA_BLOCK_SIZE);
    size_t isize = size;
    uint32_t old = 0;
    int err;
    
    err = disk_device_erase(cdev->dst, dofs, erase_size);
    if (err) {
        pr_err("%s erasing address(%x, %x) failed(%d)\n", __func__, dofs, erase_size, err);
        return err;
    }

    while (size > 0) {
        size_t bytes = rte_min(size, sizeof(global_buffer));
        err = disk_device_read(cdev->src, global_buffer, bytes, sofs);
        if (err) {
            pr_err("%s read address(%x, %x) failed(%d)\n", __func__, sofs, bytes, err);
            return err;
        }
        
        err = disk_device_write(cdev->dst, global_buffer, bytes, dofs);
        if (err) {
            pr_err("%s write address(%x, %x) failed(%d)\n", __func__, dofs, bytes, err);
            return err;
        }
        
        sofs += bytes;
        dofs += bytes;
        size -= bytes;

        copy_notify(cdev, name, isize, size, &old);
    }

    return 0;
}

static int
file_write(struct disk_device *dev, uint32_t ofs, 
    const void *buf, size_t size) {
    size_t erase_size = RTE_ALIGN(size, MEDIA_BLOCK_SIZE);
    int err;

    err = disk_device_erase(dev, ofs, erase_size);
    if (err) {
        pr_err("%s erasing address(%x, %x) failed(%d)\n", __func__, 
            ofs, erase_size, err);
        return err;
    }

    err = disk_device_write(dev, buf, size, ofs);
    if (err)
        pr_err("%s write address(%x, %x) failed(%d)\n", __func__, 
            ofs, size, err);

    return err;
}

static int
fw_package_check(struct copy_device *cdev, struct package_header *pheader,
    struct fwpkg_record *pi) {
    struct file_header *header = &pheader->base;
    uint32_t crc = 0;
    int err;

    pr_dbg("read package data from 0x%x\n", pi->dl_offset);
    err = disk_device_read(cdev->src, pheader, sizeof(*pheader),
        pi->dl_offset);
    if (err)
        return err;

    if (header->magic != FILE_HMAGIC ||
        header->nums > MAX_FWPACK_FILES) {
        pr_err("Not found firmware package (magic: 0x%x, nums: %d)\n", 
            header->magic, header->nums);
        return -EINVAL;
    }

    for (uint32_t i = 0; i < header->nums; i++) {
        err = file_checksum(&crc, cdev->src, 
            pi->dl_offset + header->headers[i].f_offset, 
            header->headers[i].f_size);
        if (err)
            return err;
    }

    if (crc != header->crc) {
        pr_err("firmware package verify failed! (crc: 0x%x)\n", crc);
        return -ENODATA;
    }

    return 0;
}

static int
fw_package_record_read(struct copy_device *cdev, struct fwpkg_record *pi) {
    uint32_t ioffset = cdev->ioffset;
    uint32_t crc;
    int err;

    err = disk_device_read(cdev->src, pi, sizeof(*pi), ioffset);
    if (err)
        return err;

    pr_dbg("read address(0x%x) pi->magic(0x%x)\n", ioffset, pi->magic);
    if (pi->magic != FILE_HMAGIC) {
        pr_warn("current package information is invalid\n");
        return -EINVAL;
    }
    
    crc = crc32_calc(0, (uint8_t *)pi, sizeof(*pi) - sizeof(uint32_t));
    if (crc != pi->hcrc) {
        pr_warn("current package information check failed\n");
        return -ENODATA;
    }

    if (pi->count > MAX_FWPACK_FILES) {
        pr_err("the number of package partition is too large\n");
        return -EBADF;
    }

    pr_notice("package download address is 0x%x (size: 0x%x)\n", pi->dl_offset, pi->dl_size);

    return 0;
}

static int
fw_package_update(struct copy_device *cdev, struct fwpkg_record *pi,
    const struct file_header *header) {
    struct copy_node cplist[MAX_FWPACK_FILES];
    uint32_t index = 0;
    int err;

    if (header->crc == pi->dcrc) {
        pr_notice("firmware package does not to be updated\n");
        return 0;
    }

    fw_package_dump(header, pi);

    for (uint32_t i = 0; i < header->nums; i++) {
        const struct file_node *psrc = &header->headers[i];

        for (uint32_t j = 0; j < pi->count; j++) {
            struct file_node *pdst = &pi->nodes[j];
            if (strcmp(psrc->f_name, pdst->f_name))
                continue;

            if (psrc->f_size > pdst->f_size) {
                pr_err("%s %s is too large! (fsrc: 0x%x fdst: 0x%x)\n", 
                    __func__, psrc->f_name, psrc->f_size, pdst->f_size);
                return -E2BIG;
            }

            cplist[index].name = psrc->f_name;
            cplist[index].dst_offset = pdst->f_offset;
            cplist[index].src_offset = psrc->f_offset + pi->dl_offset;
            cplist[index].size = psrc->f_size;
            index++;
            break;
        }

        if (i + 1 != index) {
            pr_err("%s is unknown file\n", psrc->f_name);
            return -EBADF;
        }
    }

    for (uint32_t i = 0; i < index; i++) {
        struct copy_node *cp = &cplist[i];
        int try = 3;

        /* Install package */
        do {
            pr_dbg("Copy %s from %x to %x\n", cp->name, cp->src_offset, cp->dst_offset);
            err = file_copy(cdev, cp->name, cp->dst_offset, 
                cp->src_offset, cp->size);
            if (!err)
                break;
        } while(--try > 0);

        if (err) {
            pr_emerg("Installing %s failed(%d)\n", cp->name, err);
            return err;
        }
    }

    pi->dcrc = header->crc;
    pi->hcrc = crc32_calc(0, (uint8_t *)pi, sizeof(*pi) - sizeof(uint32_t));
    err = file_write(cdev->src, cdev->ioffset, pi, sizeof(*pi));
    if (err)
        pr_err("save package information failed(%d)\n", err);
    
    pr_notice("Install successful\n");

    return err;
}

int general_nboot(
    const char *ddev,
    const char *sdev,
    uint32_t    ioffset,
    void       (*boot)(void), 
    void       (*notify)(const char *, int)
) {
    struct package_header header = {0};
    struct copy_device cdev;
    struct fwpkg_record rec;
    int err;

    pr_info(
        "\n=====================================\n"
          "          General BootLoader *       \n"
          "=====================================\n"
    );
    if (!boot) {
        pr_err("Invalid boot function\n");
        return -EINVAL;
    }

    cdev.ioffset = ioffset;
    cdev.notify  = notify;

    err = disk_device_open(ddev, &cdev.dst);
    if (err) {
        pr_err("%s not found %s\n", __func__, ddev);
        goto _exit;
    }

    err = disk_device_open(sdev, &cdev.src);
    if (err) {
        pr_err("%s not found %s\n", __func__, sdev);
        goto _exit;
    }

    err = fw_package_record_read(&cdev, &rec);
    if (err)
        goto _exit;

    err = fw_package_check(&cdev, &header, &rec);
    if (err) {
        pr_err("%s firmware package is invalid\n", __func__);
        goto _exit;
    }

    err = fw_package_update(&cdev, &rec, &header.base);

_exit:
    pr_dbg("Starting firmware...\n");
    boot();
    return err;
}