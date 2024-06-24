/*
 * Copyright 2022 wtcat
 * Copyright 2023 wtcat (Modified)
 */

#ifdef CONFIG_HEADER_FILE
#include CONFIG_HEADER_FILE
#endif

#include <assert.h>
#include <stdint.h>
#include <string.h>

#include "basework/errno.h"
#include "basework/container/ahash.h"


static inline uint32_t
ahash_hash_key(struct hash_header *header, uintptr_t addr) {
    return ((addr ^ ((uintptr_t)(addr) >> header->logsize)) & header->logmask);
}

static inline struct hash_node *
node_alloc(struct hash_header *header) {
    char *newnode = header->freelist;
    if (newnode) 
        header->freelist = *(char **)newnode;
    assert(newnode != NULL);
    return (struct hash_node *)newnode;
}

static inline void 
node_free(struct hash_header *header, struct hash_node *node) {
    char *p = (char *)node;
    *(char **)p = header->freelist;
    header->freelist = p;
}

int 
ahash_add(struct hash_header *header, const void *key,
    struct hash_node **pnode) {
    struct hash_node *newn = node_alloc(header);
    if (rte_likely(newn)) {
        uint32_t ofs = ahash_hash_key(header, (uintptr_t)key);
        rte_hlist_add_head(&newn->node, &header->slots[ofs].head);
#ifdef AHASH_DEBUG
        header->slots[ofs].hits++;
#endif
        newn->key = key;
        if (pnode)
            *pnode = newn;
        return 0;
    }
    return -ENOMEM;
}

struct hash_node *
ahash_find(struct hash_header *header, void *key) {
    uint32_t ofs = ahash_hash_key(header, (uintptr_t)key);
    struct rte_hlist *head = &header->slots[ofs].head;

    for (struct rte_hnode *pos = head->first; 
        pos; pos = pos->next) {
        struct hash_node *p = rte_container_of(pos, struct hash_node, node);
        if (p->key == key)
            return p;
    }

    return NULL;
}

void 
ahash_del(struct hash_header *header, struct hash_node *p) {
    rte_hlist_del(&p->node);
    node_free(header, p);
}

void 
ahash_visit(struct hash_header *header, 
    bool (*visitor)(struct hash_node *, void *), void *arg) {
    int lgsz = 1 << header->logsize;
    struct rte_hnode *pos;

    if (!header || !visitor)
        return;

    for (int i = 0; i < lgsz; i++) {
        rte_hlist_foreach(pos, &header->slots[i].head) {
            struct hash_node *p = rte_container_of(pos, struct hash_node, node);
            if (!visitor(p, arg))
                break;
        }
    }
}

int ahash_init(struct hash_header *header, void *buffer, 
    size_t size, size_t node_size, size_t logsz) {
    size_t bsize;

    if (header == NULL || buffer == NULL)
        return -EINVAL;

    if (node_size < sizeof(struct hash_node))
        return -EINVAL;

    if (logsz == 0)
        logsz = 4;

    if (logsz & (logsz - 1))
        return -EINVAL;

    bsize = (1 << logsz) * sizeof(struct hash_slot);
    if (size < node_size + bsize)
        return -EINVAL;

    memset(header, 0, sizeof(*header));
    memset(buffer, 0, size);

    char *ptr = (char *)buffer;
    size_t maxitems = (size - bsize) / node_size;

    /* Initialize free-node list */
    for (size_t i = 0; i < maxitems; i++) {
        *(char **)ptr = header->freelist;
        header->freelist = ptr;
        ptr += node_size;
    }

    header->freends = (struct hash_node *)buffer;
    header->slots = (struct hash_slot *)ptr;
    header->size = maxitems;
    header->logsize = logsz;
    header->logmask = (1 << logsz) - 1;

    return 0;
}
