/*
 * Copyright 2024 wtcat
 */

#ifdef CONFIG_HEADER_FILE
#include CONFIG_HEADER_FILE
#endif

#define pr_fmt(fmt) "<workout_dataimpl>: "fmt
#define CONFIG_LOGLEVEL LOGLEVEL_DEBUG
#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include "basework/errno.h"
#include "basework/os/osapi.h"
#include "basework/generic.h"
#include "basework/errno.h"
#include "basework/log.h"
#include "basework/assert.h"
#include "basework/malloc.h"
#ifndef _MSC_VER
#include "basework/rte_atomic.h"
#endif
#include "basework/container/queue.h"
#include "basework/lib/crc.h"
#include "basework/lib/string.h"

#ifdef CONFIG_SIMULATOR
#define lz4_namespace_h_
#endif
#include "basework/thirdparty/lz4/lz4.h"
#include "basework/misc/workout_spdata.h"
#include "basework/misc/workout_dataimpl.h"

#define SPORT_DEBUG 1

struct bd_header {
    uint16_t    comp;
    uint16_t    fragnum;
    uint32_t    size;
};

struct spfile {
    struct bd_header bd;
    os_file_t        fd;
    void            *cache;
    size_t           csize;
};

struct spmain_file {
    void          *fd;
    SportDatabase  db;
    uint32_t       nremain;
    uint32_t       nbufs;
};

struct comp_header {
    uint32_t  src_size;
    uint32_t  dst_size;
};

struct data_queue {
    const char      *name;
    char            *buffer[2];
    uint32_t         offset[2];
    uint32_t         bufsize;
    uint32_t         putidx;
    uint32_t         getidx;
    struct bd_header bd;
};

struct file_writer {
#if SPORT_DEBUG
    struct data_queue *q;
#endif
    char              *buf;
    os_file_t          fd;
    size_t             len;
    TAILQ_ENTRY(file_writer) node;
};

#ifdef _MSC_VER
#undef RTE_ALIGN
#define RTE_ALIGN(v, a) (((v) + (a) - 1) & ~((a) - 1))

#undef rte_min_t
#define rte_min_t(type, x, y) (type)(((type)(x) < (type)(y))? (x) : (y))

#define EBADE 300
#else  /* !_MSC_VER */
STATIC_ASSERT(sizeof(os_sem_t) <= SPORT_SEM_STRUCT_SIZE, " ");
#endif /* _MSC_VER */

static int spfile_remove_old(const char *dir);

static struct data_queue *queue_create(const char *name, size_t size) {
    size_t align = sizeof(void *);
    struct data_queue *dq;

    size = RTE_ALIGN(size, align);
    dq = general_calloc(1, RTE_ALIGN(sizeof(*dq), align) + (size << 1));
    if (dq == NULL)
        return NULL;

    dq->name      = name;
    dq->buffer[0] = (char *)RTE_ALIGN((uintptr_t)(dq + 1), align);
    dq->buffer[1] = dq->buffer[0] + size;
    dq->bufsize   = size;
    dq->getidx    = 1;
    dq->putidx    = 0;

    return dq;
}

static int queue_destroy(struct data_queue **dq) {
    general_free(*dq);
    *dq = NULL;
    return 0;
}

static void queue_swapbuf(struct data_queue *q) {
    pr_dbg("queue(%s) swap buffer (curr: %d, next: %d)\n", q->name, 
        q->offset[q->putidx], q->offset[q->getidx]);
    rte_swap_t(uint32_t, q->getidx, q->putidx);
    q->offset[q->putidx] = 0;
}

static int queue_input(struct data_queue *q, const void *p, size_t size) {
    uint32_t idx = q->putidx;
    uint32_t ofs = q->offset[idx];

    if (rte_likely(ofs + size < q->bufsize)) {
        memcpy(q->buffer[idx] + ofs, p, size);
        q->offset[idx] = ofs + size;
        return 0;
    }

    rte_assert(q->offset[q->getidx] == 0);
    queue_swapbuf(q);
    idx = q->putidx;
    ofs = q->offset[idx];
    memcpy(q->buffer[idx] + ofs, p, size);
    q->offset[idx] = ofs + size;

    return 1;
}

static size_t queue_get_rawdata(struct data_queue *q, void **ptr) {
    uint32_t idx = q->getidx;
    size_t   len = q->offset[idx];
    if (len > 0) {
        *ptr = q->buffer[idx];
        q->offset[idx] = 0;
    }
    return len;
}

static ssize_t queue_get_data(struct data_queue *q, void **buf) {
    void  *qbuf;
    size_t qlen;

    qlen = queue_get_rawdata(q, (void **)&qbuf);
    if (q->bd.comp == SPORT_COMP_LZ4 && qlen > 0) {
        struct comp_header *p;
        int  len;

        p = general_malloc(q->bufsize + sizeof(*p));
        if (p == NULL) 
            return -ENOMEM;

#ifdef CONFIG_SIMULATOR
		len = LZ4_compress_fast(qbuf, (char *)(p + 1), qlen, q->bufsize, 1);
#else
        len = lz4_compress_fast(qbuf, (char *)(p + 1), qlen, q->bufsize, 1);
#endif
        if (len == 0) {
            pr_err("compress %d bytes failed(%d)\n", qlen, len);
            general_free(p);
            return -EBADE;
        }

        pr_dbg("compress data from %d bytes to %d bytes\n", qlen, len);
        p->src_size = qlen;
        p->dst_size = len;
        qlen = len + sizeof(*p);
        q->bd.fragnum++;
        q->bd.size += qlen;
        memcpy(qbuf, p, qlen);
        general_free(p);
    }

    *buf = qbuf;
    return qlen;
}

static ssize_t file_write_wrapper(os_file_t fd, const void *buf, size_t size) {
    int ret;

    ret = vfs_write(fd, buf, size);
    if (rte_unlikely(ret < 0)) {
        if (ret == -ENOMEM) {
            spfile_remove_old(SPFILE_ROOT);
            ret = vfs_write(fd, buf, size);
            if (ret < 0)
                return ret;
        }
    }

    return 0;
}

static int fwriter_submit(struct sport_record *sdr, struct data_queue *q, 
    os_file_t fd) {
    struct file_writer *writer;
    int len;
 
    pr_dbg("%s %s\n", __func__, q->name);
    writer = general_malloc(sizeof(struct file_writer));
    if (writer == NULL) {
        pr_err("%s allocate %d bytes failed\n", __func__, 
            sizeof(struct file_writer));
        return -ENOMEM;
    }

    len = queue_get_data(q, (void **)&writer->buf);
    if (len < 0) {
        general_free(writer);
        return len;
    }

    writer->len = len;
    writer->fd  = fd;
#if SPORT_DEBUG
    writer->q   = q;
#endif
    TAILQ_INSERT_TAIL(&sdr->sync_list, writer, node);
    return 0;
}

static int fwriter_poll(struct sport_record *sdr, size_t maxbytes) {
    struct file_writer *writer = TAILQ_FIRST(&sdr->sync_list);
    int ret;

    if (writer != NULL) {
        size_t bytes = rte_min_t(size_t, writer->len, maxbytes);
        ret = file_write_wrapper(writer->fd, writer->buf, bytes);
        if (ret < 0) {
            pr_err("fwriter_poll write file failed\n");
            goto _remove;
        }

#if SPORT_DEBUG
        pr_notice("fwriter_poll(%s): writen %d bytes to file\n", 
            writer->q->name, bytes);
#endif

        writer->buf += bytes;
        writer->len -= bytes;
        if (writer->len == 0) {
            ret = 0;
            goto _remove;
        }
    }
    return 0;

_remove:
    TAILQ_REMOVE(&sdr->sync_list, writer, node);
    general_free(writer);
    return ret;
}

static int sport_flush_queue(struct data_queue *q, os_file_t fd) {
    void  *buffer;
    int    len;
    int    err = 0;

    len = queue_get_data(q, &buffer);
    if (len > 0) {
        err = file_write_wrapper(fd, buffer, (size_t)len);
        if (err < 0)
            return err;
        err = 0;
    }

    queue_swapbuf(q);
    len = queue_get_data(q, &buffer);
    if (len > 0) {
        err = file_write_wrapper(fd, buffer, (size_t)len);
        if (err > 0)
            return 0;
    }

    return err;
}

static int sport_record_flush(struct sport_record *srd) {
    int err;

    /* If the record time is less than 1 min, Then not save it */
    if (srd->time_counter < srd->time_threshold)
        return 0;

    /* Flush all pending nodes */
    do {
        err = fwriter_poll(srd, UINT32_MAX);
        if (err)
            return err;
    } while (TAILQ_FIRST(&srd->sync_list));

    /*  */
    if (srd->timer3s_counter != 0) {
        srd->db.count++;
        queue_input(srd->main_queue, &srd->data3s, sizeof(srd->data3s));
    }

    pr_dbg("data3s counter: %d, timer3s counter: %d\n", srd->db.count, srd->timer3s_counter);

    rte_assert(TAILQ_FIRST(&srd->sync_list) == NULL);
    err = sport_flush_queue(srd->main_queue, srd->fd);
    if (err) {
        pr_err("flush main-queue failed(%d)\n", err);
        return err;
    }
    err = sport_flush_queue(srd->gps_queue, srd->fd_gps);
    if (err) {
        pr_err("flush gps-queue failed(%d)\n", err);
        return err;
    }
    
    /*
     * Record file header
     */
    err = vfs_lseek(srd->fd, 0, VFS_SEEK_SET);
    err |= vfs_write(srd->fd, &srd->db, sizeof(srd->db));
    if (err < 0) {
        pr_err("write sport-record header failed(%d)\n", err);
        return err;
    }

    err = vfs_lseek(srd->fd_gps, 0, VFS_SEEK_SET);
    err |= vfs_write(srd->fd_gps, &srd->gps_queue->bd, 
        sizeof(struct bd_header));
    return err;
}

static int sport_record_destroy(struct sport_record *srd) {
    /* 
     * Delete record timer 
     */
    os_timer_destroy(srd->timer);
    sport_record_active(srd, false);
    
    /*
     * Flush cache data to media
     */
    sport_record_flush(srd);

    /*
     * Release all resource
     */
    vfs_close(srd->fd_gps);
    vfs_close(srd->fd);
    queue_destroy(&srd->gps_queue);
    queue_destroy(&srd->main_queue);

    if (srd->time_counter < srd->time_threshold) {
        pr_info("The sport time is less than 1 minute and will not be recorded\n");
        vfs_unlink(srd->db.name);
        vfs_unlink(srd->db.gps);
    }

    os_sem_post((os_sem_t *)srd->sem);
    srd->dops = NULL;
    pr_dbg("Destroy sport-record instance\n");

    return 0;
}

static void sport_record_timer_cb(os_timer_t timer, void *arg) {
    struct sport_record *sdr = (struct sport_record *)arg;
    const SportDataOperations *dops = sdr->dops;
    SportCoordinate coord;
    int swap;
    int ret;

    if (rte_unlikely(sdr->should_stop)) {
        pr_dbg("sport recorder should stop\n");
        if (sdr->should_stop & SPORT_TIMER_CLOSE_F)
            sport_record_destroy(sdr);
        return;
    }

    /* Reset timer period to 1 second */
    os_timer_mod(timer, sdr->period_ms);

    sdr->timer3s_counter++;
    sdr->time_counter++;

    /* Get data from sport algorithem */
    dops->get_coord(&coord);
    dops->get_data3s(&sdr->data3s, sdr->timer3s_counter);

    /* Consume data */
    if (dops->process)
        dops->process(sdr, &sdr->db.data);

    /*
     * Enqueue coordinate data
     */
    pr_dbg("{%d}gps data enqueue: %d bytes\n", sdr->time_counter, sizeof(coord));
    swap = queue_input(sdr->gps_queue, &coord, sizeof(coord));
    if (rte_unlikely(swap)) {
        ret = fwriter_submit(sdr, sdr->gps_queue, sdr->fd_gps);
        if (ret < 0)
            goto _failed;
    }

    /*
     * Enqueue sport/health data 
     */
    if (sdr->timer3s_counter == 3) {
        sdr->timer3s_counter = 0;
        pr_dbg("{%d}sport data enqueue: %d bytes\n", sdr->time_counter, sizeof(sdr->data3s));
        swap = queue_input(sdr->main_queue, &sdr->data3s, sizeof(sdr->data3s));
        if (rte_unlikely(swap)) {
            ret = fwriter_submit(sdr, sdr->main_queue, sdr->fd);
            if (ret < 0)
                goto _failed;
        }
    }

    /*
     * Synchronization data to file
     */
    ret = fwriter_poll(sdr, 512);
    if (ret < 0)
        goto _failed;

    return;

_failed:
    pr_err("sport recorder error(%d)\n", ret);
    sport_record_active(sdr, false);
    sdr->error = ret;
    if (dops->perror)
        dops->perror(sdr, sdr->error);
}

int sport_record_open(const char *name, uint32_t period, size_t qsize,
    int comp, const SportDataOperations *dops, 
    struct sport_record *srd) {
    SportTime tm;
    int err;

    if (qsize < 32 || period < 10)
        return -EINVAL;

    if (name == NULL || srd == NULL)
        return -EINVAL;

    if (dops == NULL ||
        dops->process == NULL ||
        dops->get_coord == NULL || 
        dops->get_data3s == NULL ||
        dops->get_time == NULL)
        return -ENOSYS;

    if (srd->dops)
        return -EBUSY;

    memset(srd, 0, sizeof(*srd));
    err = os_timer_create(&srd->timer, sport_record_timer_cb, 
        srd, false);
    if (err)
        goto _exit;

    dops->get_time(&tm);
    snprintf(srd->db.name, sizeof(srd->db.name), 
        "%s@%04d%02d%02d%02d%02d%02d.sp", 
        name,
        tm.tm_year,
        tm.tm_mon,
        tm.tm_mday,
        tm.tm_hour,
        tm.tm_min,
        tm.tm_sec);

    err = vfs_open((os_file_t *)&srd->fd, srd->db.name, VFS_O_RDWR|VFS_O_CREAT);
    if (err)
        goto _deltimer;

    snprintf(srd->db.gps, sizeof(srd->db.gps), "%s.gps", 
        srd->db.name);
    err = vfs_open((os_file_t *)&srd->fd_gps, srd->db.gps, VFS_O_RDWR|VFS_O_CREAT);
    if (err)
        goto _close_fd1;

    srd->main_queue = queue_create(srd->db.name, qsize);
    if (srd->main_queue == NULL) {
        err = -ENOMEM;
        goto _close_fd2;
    }

    srd->gps_queue = queue_create(srd->db.gps, qsize);
    if (srd->gps_queue == NULL) {
        err = -ENOMEM;
        goto _free_mq;
    }
    srd->gps_queue->bd.comp = comp;

    err = vfs_lseek(srd->fd, sizeof(srd->db), VFS_SEEK_SET);
    if (err < 0) {
        pr_err("seek file(%s) failed(%d)", name, err);
        goto _free_sq;
    }
    
    err = vfs_lseek(srd->fd_gps, sizeof(struct bd_header), VFS_SEEK_SET);
    if (err < 0) {
        pr_err("seek file(%s.gps) failed(%d)", name, err);
        goto _free_sq;
    }

    os_sem_doinit((os_sem_t *)srd->sem, 0);
    TAILQ_INIT(&srd->sync_list);
    srd->time_threshold = 60000 / period;
    srd->period_ms = period;
    srd->should_stop = SPORT_TIMER_PAUSE_F;
    srd->dops = dops;
    srd->db.data.sp_start_time = tm;
    if (dops->init)
        dops->init(&srd->db.data);

    return 0;

_free_sq:
    queue_destroy(&srd->gps_queue);
_free_mq:
    queue_destroy(&srd->main_queue);
_close_fd2:
    vfs_close(srd->fd_gps);
_close_fd1:    
    vfs_close(srd->fd);
_deltimer:
    os_timer_destroy(srd->timer);
_exit:
    return err;
}

int sport_record_active(struct sport_record *srd, bool active) {
    if (srd == NULL)
        return -EINVAL;

    if (srd->should_stop & SPORT_TIMER_CLOSE_F)
        return -EBUSY;

    if (active) {
        srd->should_stop &= ~SPORT_TIMER_PAUSE_F;
        os_timer_mod(srd->timer, srd->period_ms);
    } else {
        srd->should_stop |= SPORT_TIMER_PAUSE_F;
    }
    return 0;
}

int sport_record_close(struct sport_record *srd, bool wait) {
    if (srd == NULL)
        return -EINVAL;

    if (srd->should_stop & SPORT_TIMER_CLOSE_F)
        return -EBUSY;

    srd->should_stop |= SPORT_TIMER_CLOSE_F;
    if (srd->should_stop & SPORT_TIMER_PAUSE_F)
        os_timer_mod(srd->timer, 1);
    
    if (wait) {
        os_sem_wait((os_sem_t *)srd->sem);
        pr_dbg("sport record closed\n");
    }
    
    return 0;
}

int spfile_open(const char *name, void **pfd) {
    struct spfile *file;
    int err;

    if (name == NULL || pfd == NULL)
        return -EINVAL;

    file = general_calloc(1, sizeof(struct spfile));
    if (file == NULL)
        return -ENOMEM;

    err = vfs_open(&file->fd, name, VFS_O_RDONLY);
    if (err)
        goto _free;

    err = vfs_read(file->fd, &file->bd, sizeof(file->bd));
    if (err < 0)
        goto _close;

    if (file->bd.size == 0) {
        err = -ENODATA;
        goto _close;
    }

    *pfd = file;
    return 0;
_close:
    vfs_close(file->fd);
_free:
    general_free(file);
    return err;
}

int spfile_close(void *fd) {
    struct spfile *file = (struct spfile *)fd;

    if (file == NULL)
        return -EINVAL;

    if (file->cache) {
        general_free(file->cache);
        file->cache = NULL;
    }
    vfs_close(file->fd);
    general_free(file);
    return 0;
}

int spfile_read(void *fd, void *buffer, size_t size) {
    struct spfile *file = (struct spfile *)fd;
    int ret;

    if (fd == NULL || buffer == NULL)
        return -EINVAL;

    if (file->bd.comp == SPORT_COMP_LZ4) {
        struct comp_header comp_hdr;
        char *p;

        ret = vfs_read(file->fd, &comp_hdr, sizeof(comp_hdr));
        if (ret <= 0)
            return ret;

        if (size < comp_hdr.src_size) {
            pr_err("file buffer require >= %d bytes\n", comp_hdr.src_size);
            return -EINVAL;
        }
        
        if (rte_likely(file->csize >= comp_hdr.dst_size)) {
            p = file->cache;
        } else {
            if (file->cache) {
                general_free(file->cache);
                file->cache = NULL;
            }
            p = general_malloc(comp_hdr.dst_size);
            if (p == NULL)
                return -ENOMEM;
            
            file->cache = p;
            file->csize = comp_hdr.dst_size;
        }
        
        ret = vfs_read(file->fd, p, comp_hdr.dst_size);
        if (ret < 0)
            return ret;

#ifdef CONFIG_SIMULATOR
		ret = LZ4_decompress_safe(p, buffer, comp_hdr.dst_size, size);
#else
        ret = lz4_decompress_safe (p, buffer, comp_hdr.dst_size, size);
#endif
        pr_dbg("decompress %d bytes -> %d bytes\n", comp_hdr.dst_size, ret);
    } else {
        ret = vfs_read(file->fd, buffer, size);
    }

    return ret;
}

static bool record_iterator(struct vfs_dirent *dirent, void *arg) {
    if (dirent->d_type == DT_REG) {
        if (strstr(dirent->d_name, ".gps"))
            return false;
        
        if (!strstr(dirent->d_name, ".sp"))
            return false;
        
        struct record_files *rf = (struct record_files *)arg;
        strlcpy(rf->name[rf->index], dirent->d_name, 64);
        rf->pname[rf->index] = rf->name[rf->index];
        rf->index++;
        if (rf->index >= MAX_SPORT_FILES)
            return true;
    }
    return false;
}

static int filename_compare(const char *s1, const char *s2) {
    int ret = 0;

    while (*s1 != '\0' && *s1 != '@') s1++;
    while (*s2 != '\0' && *s2 != '@') s2++;
    s1++;
    s2++;

    while (*s1 && *s2) {
        ret = (int)((int)*s1 - (int)*s2);
        if (ret)
            break;
        s1++;
        s2++;
    }

    return ret;
}

static int spfile_unlink_old(struct record_files *rfile) {
    int err = 0, idx = 0;

    for (int i = 1; i < rfile->index; i++) {
        if (filename_compare(rfile->pname[idx], rfile->pname[i]) > 0)
            idx = i;
    }

    if (rfile->index > 0) {
        /* Remove files */
        pr_dbg("removing %s\n", rfile->pname[idx]);
        err = vfs_unlink(rfile->pname[idx]);
        if (!err) {
            strlcat(rfile->pname[idx], ".gps", 64);
            err = vfs_unlink(rfile->pname[idx]);
            while (idx < rfile->index - 1) {
                rfile->pname[idx] = rfile->pname[idx + 1];
                idx++;
            }
            rfile->index--;
            rfile->pname[rfile->index] = NULL;
        }
    }

    return err;
}

static int spfile_collect_files(const char *dir, struct record_files *r) {
    memset(r, 0, sizeof(*r));
    return vfs_dir_foreach(dir, record_iterator, r);
}

int spfile_constrain(const char *dir, int maxfiles) {
    struct record_files rfile;
    int err;

    err = spfile_collect_files(dir, &rfile);
    if (err)
        return err;

    while (rfile.index > maxfiles) {
        err = spfile_unlink_old(&rfile);
        if (err)
            break;
    }
    
    return err;
}

static int spfile_remove_old(const char *dir) {
    struct record_files rfile;
    int err;

    err = spfile_collect_files(dir, &rfile);
    if (err)
        return err;

    return spfile_unlink_old(&rfile);
}

int spmain_file_open(const char *path, uint32_t rdburst, void **pfp) {
    struct spmain_file *sf;
    int err;

    if (path == NULL || pfp == NULL)
        return -EINVAL;
    
    if (rdburst == 0)
        rdburst = 100;
    
    sf = general_calloc(1, sizeof(*sf) + sizeof(SportRealtimeData3s)*rdburst);
    if (sf == NULL)
        return -ENOMEM;
    
    err = vfs_open((os_file_t *)&sf->fd, path, VFS_O_RDONLY);
    if (err)
        goto _free;

    err = vfs_read(sf->fd, &sf->db, sizeof(sf->db));
    if (err < 0)
        goto _close;

    if (sf->db.count == 0) {
        err = -ENODATA;
        goto _close;
    }

    sf->nbufs = rdburst;
    sf->nremain = sf->db.count - 1;
    *pfp = sf;
    return 0;

_close:
    vfs_close(sf->fd);
_free:
    general_free(sf);
    return err;
}

int spmain_file_close(void *fp) {
    struct spmain_file *sf = (struct spmain_file *)fp;

    if (sf == NULL)
        return -EINVAL;
    vfs_close(sf->fd);
    general_free(sf);
    return 0;
}

int spmain_file_getdb(void *fp, SportDataInstance **pdb) {
    struct spmain_file *sf = (struct spmain_file *)fp;

    if (sf == NULL || pdb == NULL)
        return -EINVAL;
    
    *pdb = &sf->db.data;
    return 0;
}

int spmain_file_walk(void *fp, bool restart,
    bool (*fread)(const SportRealtimeData3s *d3s, uint32_t nitem, void *param),
    void *param) {
    struct spmain_file *sf = (struct spmain_file *)fp;
    SportRealtimeData3s *p = (SportRealtimeData3s *)(sf + 1);
    int ret;

    if (sf == NULL)
        return -EINVAL;
    
    if (restart) {
        sf->nremain = sf->db.count - 1;
        ret = vfs_lseek(sf->fd, sizeof(sf->db), VFS_SEEK_SET);
        if (ret < 0)
            return ret;
    }

    if (sf->nremain > 0) {
        do {
            uint32_t nitem = rte_min_t(uint32_t, sf->nbufs, sf->nremain);
            ret = vfs_read(sf->fd, p, nitem * sizeof(*p));
            if (ret < 0)
                return ret;

            rte_assert(ret == nitem * sizeof(*p));
            sf->nremain -= nitem;
            if (fread(p, nitem, param))
                return 0;

        } while (sf->nremain > 0);

        /*
         * Process last item
         */
        ret = vfs_read(sf->fd, p, sizeof(*p));
        if (ret < 0)
            return ret;
        
        for (uint32_t i = sf->db.hrlast_count; i < 3; i++)
            p->heartrate[i] = 0;
        fread(p, 1, param);
    }
    
    return 0;
}

static int file_read_named(const char *path, void *buffer, size_t size) {
    os_file_t fd;
    int err;

    err = vfs_open(&fd, path, VFS_O_RDONLY);
    if (err) {
        pr_err("Open file(%s) failed\n", path);
        goto _exit;
    }

    err = vfs_read(fd, buffer, size);
    if (err < 0) {
        pr_err("Read file(%s) failed\n", err);
        goto _close;
    }
    err = 0;
_close:
    vfs_close(fd);
_exit:
    return err;
}

static int file_write_named(const char *path, const void *buffer, size_t size) {
    os_file_t fd;
    int err;

    err = vfs_open(&fd, path, VFS_O_CREAT|VFS_O_WRONLY);
    if (err) {
        pr_err("Open file(%s) failed(%d)\n", path, err);
        goto _exit;
    }

    err = vfs_write(fd, buffer, size);
    if (err < 0) {
        pr_err("Write file(%s) failed(%d)\n", path, err);
    }

    vfs_close(fd);
_exit:
    return err;
}

int sport_general_load(struct spdata_object *o) {
    struct sport_recdata *p;
    uint16_t chksum;
    int err;

    if (o->private == NULL) {
        p = general_calloc(1, sizeof(*p) + o->osize);
        if (p == NULL) {
            pr_err("No more memory\n");
            return -ENOMEM;
        }
        p->data = (SportDataHeader *)(p + 1);
        o->private = p;
    } else {
        p = o->private;
    }

    err = file_read_named(o->path, p->data, o->osize);
    if (err)
        goto _reset;

    chksum = lib_crc16((void *)p->data->buffer, 
        o->osize - sizeof(SportDataHeader));
    if (chksum != p->data->chksum) {
        pr_err("CRC check failed\n");
        goto _reset;
    }

    return 0;
_reset:
    return o->reset(o, p->data);
}

int sport_general_unload(struct spdata_object *o) {
    struct sport_recdata *p = o->private;
    int ret = 0;

    if (p->dirty) {
        p->data->chksum = lib_crc16((void *)p->data->buffer, 
            o->osize - sizeof(SportDataHeader));
        ret = file_write_named(o->path, p->data, o->osize);
    }

    if (ret > 0) {
        general_free(p);
        o->private = NULL;
        ret = 0;
    }

    return ret;
}
