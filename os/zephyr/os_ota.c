/*
 * Copyright 2023 wtcat
 */

#ifdef CONFIG_HEADER_FILE
#include CONFIG_HEADER_FILE
#endif

#define CONFIG_LOGLEVEL  LOGLEVEL_DEBUG
#define pr_fmt(fmt) "os_ota: " fmt

#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <init.h>
#include <partition/partition.h>
#include <sdfs.h>

#include "basework/assert.h"
#include "basework/lib/fnmatch.h"
#include "basework/dev/blkdev.h"
#include "basework/dev/disk.h"
#include "basework/dev/partition.h"
#include "basework/dev/gpt.h"
#include "basework/utils/ota_fstream.h"
#include "basework/malloc.h"
#include "basework/assert.h"
#include "basework/log.h"
#include "basework/boot/boot.h"
#include "basework/lib/string.h"
#include "basework/utils/binmerge.h"
#include "basework/lib/crc.h"

#ifndef _MB
#define _MB(n) ((n) * 1024 * 1024ul)
#endif

#define OTA_STORAGE_MEDIA "spi_flash"

struct file_partition {
    struct disk_device *dd;
    uint32_t offset;
    uint32_t len;
    uint32_t file_size;
    uint32_t file_blksize;
    uint32_t dirty_offset;
    uint32_t max_erasesize;
    uint32_t erase_mask;
    const char *name;
};

struct ram_parition {
    const char *name;
    size_t fsize;
    char buffer[];
};

struct ota_region {
    uint32_t begin;
    uint32_t end;
};

#define ota_get_partition(a, b) partition_get_part(b)

static const struct ota_fstream_ops ram_fstream_ops;
static const struct ota_fstream_ops fstream_ops;
static bool res_first = true;
static struct file_partition file_dev;
static struct partition_entry resext_entry = {
    .name = {"res_ext"},
    .type = 0,
    .file_id = 11,
    .mirror_id = 0,
    .storage_id = 0,
    .flag = 0,
    .size = 0x30000,
};


//Picture resource extension partition
const struct partition_entry *partition_get_stf_part_ext(
	u8_t stor_id, u8_t file_id) {
  //16MB nor
    if (resext_entry.storage_id == stor_id && 
        resext_entry.file_id == file_id &&
        res_first) {
        return &resext_entry;
    }
    return NULL;
}

#define ota_partition_get(a, b)  __ota_partition_get(a, b, &file_dev)
#define ota_partition_get_nodd(a, b) __ota_partition_get(a, b, NULL)

static const char *part_to_devname(uint8_t storage) {
    switch(storage) {
    case STORAGE_ID_NOR:
        return "spi_flash";
    case STORAGE_ID_NAND:
        return "spinand";
    case STORAGE_ID_DATA_NOR:
        return "spi_flash_2";
    default:
        rte_assert0(0);
        return NULL;
    }
}

static struct partition_entry *__ota_partition_get(uint8_t unused, uint8_t file_id, 
    struct file_partition *fp) {
    const struct partition_entry *parti;

    (void) unused;
    parti = parition_get_entry(file_id);
    rte_assert(parti != NULL);

    if (fp) {
        disk_device_open(part_to_devname(parti->storage_id), &fp->dd);
        rte_assert(file_dev.dd != NULL);
    }

    return (struct partition_entry*)parti;
}

void resource_ext_check(void) {
#define PARTITION_OFS(x) (fw->offset + (x))
    const struct disk_partition *fw; 
    const struct partition_entry *part;

    fw = disk_partition_find("firmware");
    rte_assert(fw != NULL);

    part = ota_partition_get(STORAGE_ID_NOR, 
        PARTITION_FILE_ID_SYSTEM);
    rte_assert(part != NULL);

    if (part->size < 0x1c2000)
        resext_entry.offset = PARTITION_OFS(0x1c2000);
    else
        resext_entry.offset = PARTITION_OFS(part->size);
    resext_entry.file_offset = resext_entry.offset;
    pr_notice("ext-resource offset(0x%x) size(0x%x) fw->offset(0x%x) resext_entry.offset(0x%x)\n", 
        part->offset, part->size, fw->offset, resext_entry.offset);
    if (sdfs_verify("/NOR:B")) {
        res_first = false;
		sdfs_verify("/NOR:B");
	}
}

static uint32_t crc32_update(uint32_t crc, const uint8_t *data, 
    size_t len) {
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

static uint32_t crc32_calc(const uint8_t *data, size_t len) {
    return crc32_update(0, data, len);
}

static int
file_partition_get_from_gpt(const char *name, struct file_partition *fpt) {
    const struct gpt_entry *gpe;
    gpe = gpt_find_by_filename(name);
    if (gpe) {
        fpt->dd = disk_device_find(gpe->parent);
        rte_assert(fpt->dd != NULL);
        pr_dbg("Opened %s partition: start(0x%x) size(%d)\n", 
            gpe->name, file_dev.offset, file_dev.len);
        fpt->offset = gpe->offset;
        fpt->len = gpe->size;
        fpt->name = name;
        return 0;
    }
    return -ENODATA;
}

static int extract_addr(const char *str, uint32_t *addr) {
    const char *beg = str;
    char *end;

    while (!isdigit((int)*beg)) {
        if (*beg == '\0')
            return -EINVAL;
        beg++;
    }

    *addr = strtoul(beg, &end, 16);
    return 0;
}

static int generate_ota_nlog(const struct disk_partition *dp, 
    const struct disk_partition *fw) {
    struct fwpkg_record rec;
    const struct partition_entry *pe;
    const struct gpt_entry *gpe;
    int err = 0, idx = 0;

    memset(&rec, 0, sizeof(rec));
    rec.magic = FILE_HMAGIC;
    rec.dl_offset = fw->offset;
    rec.dl_size = fw->len;

    /*
     * Global parition table
     */
    pe = ota_partition_get_nodd(STORAGE_ID_NOR, 30);
    if (pe != NULL) {
        strlcpy(rec.nodes[idx].node.f_name, "gpt.bin", MAX_NAMELEN);
        strlcpy(rec.nodes[idx].d_device, part_to_devname(pe->storage_id), MAX_NAMELEN);
        strlcpy(rec.nodes[idx].s_device, fw->parent, MAX_NAMELEN);
        rec.nodes[idx].node.f_offset = pe->offset;
        rec.nodes[idx].node.f_size   = pe->size;
        idx++;
    }

    /*
     * Firmware
     */
    pe = ota_partition_get_nodd(STORAGE_ID_NOR, 4);
    if (pe != NULL) {
        strlcpy(rec.nodes[idx].node.f_name, "zephyr.bin", MAX_NAMELEN);
        strlcpy(rec.nodes[idx].d_device, part_to_devname(pe->storage_id), MAX_NAMELEN);
        strlcpy(rec.nodes[idx].s_device, fw->parent, MAX_NAMELEN);
        rec.nodes[idx].node.f_offset = pe->offset;
        rec.nodes[idx].node.f_size   = pe->size;
        idx++;
    }

    /*
     * The sub-parition files list
     */
    static const char *bin_array[] = {
        "fwparam.bin", /* system parameters */
        "res+.bin",    /* Increase resource for picture */
        "fonts+.bin",  /* Increase resource for font */
        NULL
    };

    for (int base = idx; idx < MAX_FWPACK_FILES && bin_array[idx - base]; idx++) {
        gpe = gpt_find_by_filename(bin_array[idx - base]);
        if (gpe != NULL) {
            strlcpy(rec.nodes[idx].node.f_name, bin_array[idx - base], MAX_NAMELEN);
            strlcpy(rec.nodes[idx].d_device, gpe->parent, MAX_NAMELEN);
            strlcpy(rec.nodes[idx].s_device, fw->parent, MAX_NAMELEN);
            rec.nodes[idx].node.f_offset = gpe->offset;
            rec.nodes[idx].node.f_size   = gpe->size;
        }
    }
  
    rte_assert0(idx <= MAX_FWPACK_FILES);
    rec.count = idx;
    rec.hcrc = crc32_calc((uint8_t *)&rec, sizeof(rec) - sizeof(uint32_t));
#ifndef CONFIG_SPINAND_ACTS
    err = disk_partition_erase_all(dp);
    err |= disk_partition_write(dp, 0, &rec, sizeof(rec));
    rte_assert(err == 0);
#else
    err = lgpt_write(dp, 0, &rec, sizeof(rec));
    blkdev_sync();
    rte_assert(err > 0);
#endif
    
    pr_dbg("updated ota information\n");
    return err;
}

static int generate_ota_log(const struct disk_partition *dp, 
    const struct disk_partition *fw) {
    struct firmware_pointer fwinfo = {0};

    /* Record firmware address information */
    // disk_partition_read(dp, 0, &fwinfo, sizeof(fwinfo));

    pr_info("Firmware information record address(0x%08x)\n", dp->offset);
    fwinfo.fh_magic = FH_MAGIC;
    fwinfo.fh_devid = 0; //ota_fstream_get_devid();
    fwinfo.addr.fw_offset = fw->offset;
    fwinfo.addr.fw_size = fw->len;

    /* Calculate check sum */
    const uint8_t *p = (const uint8_t *)&fwinfo.addr;
    uint8_t chksum = 0;
    for (size_t i = 0; i < offsetof(struct firmware_addr, chksum); i++) 
        chksum ^= p[i];
    fwinfo.addr.chksum = chksum;

#ifndef CONFIG_SPINAND_ACTS
    disk_partition_erase_all(dp);
    disk_partition_write(dp, 0, &fwinfo, sizeof(fwinfo));

    //TODO:
    struct firmware_pointer fwchk;
    disk_partition_read(dp, 0, &fwchk, sizeof(fwchk));
    if (fwchk.addr.fw_offset != fwinfo.addr.fw_offset)
        rte_assert(0);

#else /* !CONFIG_SPINAND_ACTS */
    blkdev_sync();
    int err = lgpt_write(dp, 0, &fwinfo, sizeof(fwinfo));
    rte_assert(err > 0);
    (void) err;
    blkdev_sync();
#endif /* CONFIG_SPINAND_ACTS */
    
    return 0;
}

static struct file_partition *
__partition_open(const char *name, size_t fsize) {
    const struct partition_entry *parti;

    file_dev.dd = disk_device_find(OTA_STORAGE_MEDIA);
    rte_assert(file_dev.dd != NULL);

    /* Just only for json parition(the sub-parition table) */
    if (!file_partition_get_from_gpt(name, &file_dev))
        return &file_dev;

    if (!fnmatch("0x*.bin", name, FNM_CASEFOLD)) {
        uint32_t start;
        if (!extract_addr(name, &start)) {
            file_dev.offset = start;
            file_dev.len = _MB(32);
            file_dev.name = name;
            pr_dbg("open %s partition: start(0x%x) size(%d)\n", 
                name, file_dev.offset, file_dev.len);
            return &file_dev;
        }
    }
    
    /* System configuration */
    if (!strcmp(name, "sdfs.bin")) {
        parti = ota_partition_get(STORAGE_ID_NOR, 
            PARTITION_FILE_ID_SDFS);
        rte_assert(parti != NULL);
        file_dev.offset = parti->offset;
        file_dev.len = parti->size;
        file_dev.name = "sdfs";
        pr_dbg("open sdfs partition: start(0x%x) size(%d)\n", 
            file_dev.offset, file_dev.len);
        return &file_dev;
    }

    if (!strcmp(name, "sdfs_k.bin")) {
        parti = ota_partition_get(STORAGE_ID_NOR, 
            20);
        rte_assert(parti != NULL);
        file_dev.offset = parti->offset;
        file_dev.len = parti->size;
        file_dev.name = "sdfs_k";
        pr_dbg("open sdfs_k partition: start(0x%x) size(%d)\n", 
            file_dev.offset, file_dev.len);
        return &file_dev;
    }

    if (!strcmp(name, "mbrec.bin")) {
        parti = ota_partition_get(STORAGE_ID_NOR, 1);
        rte_assert(parti != NULL);
        file_dev.offset = parti->offset;
        file_dev.len = parti->size;
        file_dev.name = "mbrec";
        pr_dbg("open mbrec partition: start(0x%x) size(%d)\n", 
            parti->offset, parti->size);
        return &file_dev;
    }

    if (!strcmp(name, "gpt.bin")) {
        pr_dbg("*** open gpt partition ***\n");
        ota_fstream_set_ops(&ram_fstream_ops);
        return ram_fstream_ops.open(name, fsize);
    }

    /* Firmware partition */
    if (!strcmp(name, "zephyr.bin") ||
        !strcmp(name, "fwpack.bin")) {
        const struct disk_partition *dp, *fdp;
        dp = disk_partition_find("firmware");
        rte_assert(dp != NULL);
        file_dev.dd = disk_device_find_by_part(dp);
        file_dev.offset = dp->offset;
        file_dev.len = dp->len;
        file_dev.name = "firmware";
        pr_dbg("open firmware partition: start(0x%x) size(%d)\n", 
            dp->offset, dp->len);
        
        if (fsize > 0) {
            fdp = disk_partition_find("firmware_cur");
            rte_assert(fdp != NULL);

            if (!strcmp(name, "fwpack.bin")) {
                pr_notice("generate firmware package information\n");
                generate_ota_nlog(fdp, dp);
            } else {
                pr_notice("generate firmware information\n");
                generate_ota_log(fdp, dp);
            }
        }

        return &file_dev;
    }
		
    if (!strcmp(name, "msgfile.bin")) {
        const struct disk_partition *dp;
        dp = disk_partition_find("msgfile.db");
        rte_assert(dp != NULL);
        file_dev.dd = disk_device_find_by_part(dp);
        file_dev.offset = dp->offset;
        file_dev.len = dp->len;
        file_dev.name = "msgfile";
        pr_dbg("open msgfile.db partition: start(0x%x) size(%d)\n", 
            dp->offset, dp->len);
        return &file_dev;
    }

    if (!strcmp(name, "bream.bin")) {
        parti = ota_partition_get(STORAGE_ID_NOR, 43);
        rte_assert(parti != NULL);
        file_dev.offset = parti->offset;
        file_dev.len = parti->size;
        file_dev.name = "gps_brm";
        pr_dbg("open gps_brm partition: start(0x%x) size(%d)\n", 
            parti->offset, parti->size);
		return &file_dev;
	}

    if (!strcmp(name, "recovery.bin")) {
        parti = ota_partition_get(STORAGE_ID_NOR, 3);
        rte_assert(parti != NULL);
        file_dev.offset = parti->offset;
        file_dev.len = parti->size;
        file_dev.name = "recovery";
        pr_dbg("open bootloader partition: start(0x%x) size(%d)\n", 
            parti->offset, parti->size);
        return &file_dev;
    }
	
    if (!strcmp(name, "dial_default.bin")) {
        const struct disk_partition *dp;
        dp = disk_partition_find("dial_local");
        rte_assert(dp != NULL);
        file_dev.dd = disk_device_find_by_part(dp);
        file_dev.offset = dp->offset;
        file_dev.len = dp->len;
        file_dev.name = "dial_local";
        pr_dbg("open dial_local partition: start(0x%x) size(%d)\n", 
            dp->offset, dp->len);
        return &file_dev;
    }

    return NULL;
}

static void partition_erase(struct file_partition *fp, uint32_t ofs, 
    uint32_t max_size) {
    size_t remain = fp->file_blksize - (ofs - fp->offset);
    size_t bytes = rte_min(max_size, remain);
    int err;

    pr_dbg("@%s erasing address(0x%08x) size(0x%x) ...\n", 
        disk_device_get_name(fp->dd), ofs, bytes);
    err = disk_device_erase(fp->dd, ofs, bytes);
    rte_assert(err == 0);
    (void) err;
    fp->dirty_offset += bytes;
}

static void* partition_open(const char *name, size_t fsize) {
#define MAX_NOR_ERASE_SIZE (64 * 1024)
    struct file_partition *fp;

    fp = __partition_open(name, fsize);
#ifndef CONFIG_SPINAND_ACTS
    if (fp) {
        if (fsize > 0) {
            size_t blksize;
            rte_assert(fsize <= fp->len);
            blksize = disk_device_get_block_size(fp->dd);
            fp->file_size = fsize;
            fp->file_blksize = RTE_ALIGN(fsize, blksize);
            if (fp->len % MAX_NOR_ERASE_SIZE == 0)
                fp->max_erasesize = MAX_NOR_ERASE_SIZE;
            else
                fp->max_erasesize = blksize;
            fp->erase_mask = fp->max_erasesize - 1;
            
            fp->dirty_offset = fp->offset;
            fsize = fp->offset & fp->erase_mask;
        
            if (fsize > 0) {
                fsize = fp->max_erasesize - fsize;
                pr_dbg("@the first erase size(0x%x) ...\n", fsize);
                partition_erase(fp, fp->offset, fsize);
            }
        }
    }
#endif /* CONFIG_SPINAND_ACTS */
    return fp;
}

static void partition_close(void *fp) {
    (void) fp;
    // blkdev_sync();
}

static int partition_write(void *fp, const void *buf, 
    size_t size, uint32_t offset) {
    struct file_partition *filp = fp;
    uint32_t wr_offset;

    wr_offset  = filp->offset + offset;
#ifndef CONFIG_SPINAND_ACTS
    uint32_t end_offset = wr_offset + size;

    if (end_offset > filp->offset + filp->file_size) {
        pr_err("write address is out of range(offset:0x%x size:%d "
                "partition(name:%s ofs:0x%x, size:%d))\n",
            offset, size, filp->name, filp->offset, filp->len);
        return -EINVAL;
    }

    while (end_offset > filp->dirty_offset)
        partition_erase(filp, filp->dirty_offset, filp->max_erasesize);

    int err = disk_device_write(filp->dd, buf, size, wr_offset);
    if (err < 0)
        return err;
    return size;
#else /* !CONFIG_SPINAND_ACTS */

    return blkdev_write(filp->dd, buf, size, wr_offset);
#endif /* CONFIG_SPINAND_ACTS */
}

static void partition_completed(int err, void *fp, const char *fname, 
    size_t size) {
    if (err)
        return;

    /*
     * Reload GPT information
     */
    if (!strcmp(fname, "gpt.bin")) {
        struct file_partition *filp = fp;
        size_t max_buflen = 4096;
        struct bin_header *bin;

        pr_info("partition file post process\n");
        bin = general_malloc(max_buflen);
        rte_assert0(bin != NULL);
        rte_assert0(disk_device_read(filp->dd, bin, 4096, filp->offset) == 0);
        rte_assert0(bin->magic == FILE_HMAGIC);
        rte_assert0(bin->size < max_buflen);
        rte_assert0(lib_crc32(bin->data, bin->size) == bin->crc);
        rte_assert0(gpt_load(bin->data) == 0);
        general_free(bin);
    }
}

static bool check_ota_environment(const struct file_header *header) {
#define GLOBAL_PARTITION_ADDR 0x1000
#define GLOBAL_PARTITION_SIZE 1024
    char buffer[GLOBAL_PARTITION_SIZE];
    const struct disk_partition *fp;
    struct disk_device *dd = NULL;
    uint16_t crc;

    /* Reset counter */
    fp = disk_partition_find("firmware_cur");
    rte_assert(fp != NULL);

    /*
    * Skip partition check
    */
    if (header->param == 0x0)
        return true;

    disk_device_open("spi_flash", &dd);
    rte_assert(dd != NULL);

#ifndef CONFIG_SPINAND_ACTS
    disk_device_read(dd, buffer, sizeof(buffer), GLOBAL_PARTITION_ADDR);
#else
    blkdev_read(dd, buffer, sizeof(buffer), GLOBAL_PARTITION_ADDR);
#endif

    crc = lib_crc16(buffer, sizeof(buffer));
    if (header->param != crc) {
        pr_err("Error***: firmware partition signature is 0x%04x\n", crc);
        return false;
    }

    return true;
}

static const struct ota_fstream_ops fstream_ops = {
    .open  = partition_open,
    .close = partition_close,
    .write = partition_write,
    .completed = partition_completed
};

static void* ram_partition_open(const char *name, size_t fsize) {
    struct ram_parition *rp;

    rp = general_calloc(1, sizeof(*rp) + fsize);
    if (rp) {
        rp->name = name;
        rp->fsize = fsize;
    }
    return rp;
}

static void ram_partition_close(void *fp) {
    if (fp)
        general_free(fp);
    ota_fstream_set_ops(&fstream_ops);
}

static int ram_partition_write(void *fp, const void *buf, 
    size_t size, uint32_t offset) {
    struct ram_parition *rp = fp;

    if (offset + size > rp->fsize)
        return -EFBIG;

    memcpy(&rp->buffer[offset], buf, size);
    return size;
}

static void ram_partition_completed(int err, void *fp, const char *fname, 
    size_t size) {
    if (err)
        return;

    /*
     * Reload GPT information
     */
    if (!strcmp(fname, "gpt.bin")) {
        struct ram_parition *rp = fp;
        struct bin_header *bin = (struct bin_header *)rp->buffer;

        pr_info("checking gpt.bin and load it...\n");
        rte_assert0(bin->magic == FILE_HMAGIC);
        rte_assert0(bin->size <= rp->fsize);
        rte_assert0(lib_crc32(bin->data, bin->size) == bin->crc);
        rte_assert0(gpt_load(bin->data) == 0);
    }
}

static const struct ota_fstream_ops ram_fstream_ops = {
    .open  = ram_partition_open,
    .close = ram_partition_close,
    .write = ram_partition_write,
    .completed = ram_partition_completed
};

static int __rte_notrace ota_fstream_init(const struct device *dev) {
    (void) dev;

    ota_fstream_set_ops(&fstream_ops);
    ota_fstream_set_envchecker(check_ota_environment);
    return 0;
}

SYS_INIT(ota_fstream_init, APPLICATION, 
    CONFIG_KERNEL_INIT_PRIORITY_DEFAULT);
