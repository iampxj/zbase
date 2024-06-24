/*
 * Copyright 2024 wtcat
 *
 * General hash that based linear-address
 */
#ifndef BASEWORK_CONTAINER_AHASH_H_
#define BASEWORK_CONTAINER_AHASH_H_

#include "basework/container/list.h"

#ifdef __cplusplus
extern "C"{
#endif

// #define AHASH_DEBUG 1

struct hash_header;

#define AHASH_NODE_BASE \
    struct rte_hnode node; \
    const void *key;

struct hash_node {
    AHASH_NODE_BASE
};

struct hash_slot {
    struct rte_hlist head;
#ifdef AHASH_DEBUG
    size_t hits;
#endif
};

struct hash_header {
    struct hash_slot *slots;
    uint16_t logsize;
    uint16_t logmask;
    char *freelist;
    size_t size;
    struct hash_node *freends;
};

#define AHASH_CALC_BUFSZ(nitem, logsz, nodesz) \
    ( \
        ((1 << (logsz)) * sizeof(struct hash_slot)) + \
        ((nitem) * (nodesz)) \
    )

#define AHASH_BUF_DEFINE(_name, _nitem, _logsz, _nodesz) \
    unsigned long _name[ \
        (AHASH_CALC_BUFSZ(_nitem, _logsz, _nodesz) + sizeof(long) - 1) \
        / sizeof(long) \
    ]
        

/*
 * ahash_add - add a node to hash header
 *
 * @header: hash header object
 * @key: memory address
 * @pnode: pointer to hash-node
 *
 * return 0 if success
 */
int ahash_add(struct hash_header *header, const void *key,
    struct hash_node **pnode);

/*
 * ahash_find - find hash_node based on a given key
 *
 * @header: hash header object
 * @key: memory address
 * return hash_node if success
 */
struct hash_node *ahash_find(struct hash_header *header, 
    void *key);

/*
 * ahash_del - delete hash node
 *
 * @header: hash header object
 * @ptr: pointer to hash_node
 */
void ahash_del(struct hash_header *header, struct hash_node *p);

/*
 * ahash_visit - iterate hash table
 *
 * @header: hash header object
 * @visitor: iterator callback
 */
void ahash_visit(struct hash_header *header, 
    bool (*visitor)(struct hash_node *, void *), void *arg);

/*
 * ahash_init - Initialize hash table
 *
 * @header: hash header object
 * @buffer: memory address
 * @size: memory size
 * @node_size: the size of hash node
 * @logsz: the log size of hash table 
 * return 0 if success
 */
int ahash_init(struct hash_header *header, void *buffer, 
    size_t size, size_t node_size, size_t logsz);

#ifdef __cplusplus
}
#endif
#endif /* BASEWORK_CONTAINER_AHASH_H_ */
