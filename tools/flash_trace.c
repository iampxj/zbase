/*
 * Copyright 2024 wtcat
 */

#include "basework/container/ahash.h"
#include "basework/log.h"

struct flash_trace {
    AHASH_NODE_BASE
    uint16_t erase_cnt;
    uint16_t write_cnt;
};

struct iter_param {
#define TRACE_SEQ_LEN 10
    uint32_t seq[TRACE_SEQ_LEN];
    uint32_t wcnt[TRACE_SEQ_LEN];
    const void *addr[TRACE_SEQ_LEN];
	bool all;
};

#ifdef CONFIG_FLASHTRACE_LOGSZ
#define HASH_LOGSZ CONFIG_FLASHTRACE_LOGSZ
#else
#define HASH_LOGSZ 8
#endif

#ifdef CONFIG_FLASHTRACE_ELEMNUMS
#define HASH_ELEM_NUM CONFIG_FLASHTRACE_ELEMNUMS
#else
#define HASH_ELEM_NUM 2000
#endif

static AHASH_BUF_DEFINE(hash_mem, HASH_ELEM_NUM, HASH_LOGSZ, 
    sizeof(struct flash_trace));
static struct hash_header flash_hash;

static bool flash_trace_iterator(struct hash_node *node, void *arg) {
    struct flash_trace *p = (struct flash_trace *)node;
    struct iter_param *ip = (struct iter_param *)arg;
    uint32_t max_diff = 0;
    int idx = -1;
    size_t len;

    len = sizeof(ip->seq) / sizeof(ip->seq[0]);
    for (size_t i = 0; i < len; i++) {
        if (p->erase_cnt > ip->seq[i]) {
            uint32_t diff = p->erase_cnt - ip->seq[i];
            if (max_diff < diff) {
                max_diff = diff;
                idx = i;
            }
        }
    }
    if (idx >= 0) {
        for (size_t i = 0; i < len; i++) {
            if (ip->addr[i] == p->key) {
                ip->seq[i]  = p->erase_cnt;
                ip->wcnt[i] = p->write_cnt;
                return true;
            }
        }
        ip->seq[idx]  = p->erase_cnt;
        ip->wcnt[idx] = p->write_cnt;
        ip->addr[idx] = p->key;
    }

	if (ip->all) {
		pr_out("\taddr(%p) erase(%d) write(%d)\n", p->key, 
			p->erase_cnt, p->write_cnt);
	}

    return true;
}

int flash_trace_erase(uint32_t addr, uint32_t size) {
    struct flash_trace *p;
    int err;

    p = (struct flash_trace *)ahash_find(&flash_hash, (void *)(uintptr_t)addr);
    if (p == NULL) {
        err = ahash_add(&flash_hash, (void *)(uintptr_t)addr, 
            (struct hash_node **)&p);
        if (!err) {
            p->erase_cnt = 1;
            p->write_cnt = 0;
        }
        return err;
    }
    p->erase_cnt++;
    return 0;
}

int flash_trace_write(uint32_t addr, uint32_t size) {
    uint32_t aligned_addr = addr & ~0xFFF;
    struct flash_trace *p;

    p = (struct flash_trace *)ahash_find(&flash_hash, 
        (void *)(uintptr_t)aligned_addr);
    if (p) 
        p->write_cnt++;

    return 0;
}

void flash_trace_dump(bool all) {
    static struct iter_param param = {{0}, {0}};

	if (all) {
		pr_out("\n\n** flash killer list: **\n");
		param.all = all;
	}
    ahash_visit(&flash_hash, flash_trace_iterator, &param);
	
	pr_out("\n** flash killer top %d **\n", TRACE_SEQ_LEN);
    for (int i = 0; i < TRACE_SEQ_LEN; i++) {
        pr_out("\t<%d>: addr(%p) erase(%d) write(%d)\n",
            i, param.addr[i], param.seq[i], param.wcnt[i]);
    }
}

int flash_trace_init(void) {
    return ahash_init(&flash_hash, hash_mem, sizeof(hash_mem), 
        sizeof(struct flash_trace), HASH_LOGSZ);
}
