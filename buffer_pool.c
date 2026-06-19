#include "read.h"
#include "buffer_pool.h"
#include <stdlib.h>
#include <string.h>

#define NODE_CACHE_SIZE 256

typedef struct {
    nid_t nid;
    struct f2fs_node node;
    uint8_t valid;
} node_cache_t;

typedef struct {
    struct f2fs_dentry_block block;
    uint32_t blkaddr;
    uint8_t valid;
} dentry_cache_t;

struct f2fs_buffer_pool {
    node_cache_t node_cache[NODE_CACHE_SIZE];
    dentry_cache_t dentry_cache;
    void *large_buf;
};

static inline uint32_t hash_nid(uint32_t nid) {
    return (nid ^ (nid >> 16)) & (NODE_CACHE_SIZE - 1);
}

int bufpool_init(f2fs_info_t *info) {
    struct f2fs_buffer_pool *pool = calloc(1, sizeof(*pool));
    if (!pool) return -1;
    
    pool->large_buf = malloc(64 * 1024);
    if (!pool->large_buf) {
        free(pool);
        return -1;
    }
    
    info->buffer_pool = pool;
    return 0;
}

void bufpool_free(f2fs_info_t *info) {
    if (!info || !info->buffer_pool) return;
    
    struct f2fs_buffer_pool *pool = info->buffer_pool;
    free(pool->large_buf);
    free(pool);
    info->buffer_pool = NULL;
}

void bufpool_reset(f2fs_info_t *info) {
    if (!info || !info->buffer_pool) return;
    
    struct f2fs_buffer_pool *pool = info->buffer_pool;
    memset(pool->node_cache, 0, sizeof(pool->node_cache));
    pool->dentry_cache.valid = 0;
}

struct f2fs_node* node_get(f2fs_info_t *info, nid_t nid) {
    if (!info || !info->buffer_pool) return NULL;
    
    struct f2fs_buffer_pool *pool = info->buffer_pool;
    uint32_t idx = hash_nid(nid);
    node_cache_t *entry = &pool->node_cache[idx];
    
    if (entry->valid && entry->nid == nid) {
        return &entry->node;
    }
    
    block_t blkaddr = get_node_blkaddr(info, nid);
    if (blkaddr == NULL_ADDR || blkaddr == NEW_ADDR) {
        return NULL;
    }
    
    if (dev_read_block(info, &entry->node, blkaddr) != 0) {
        return NULL;
    }
    
    entry->nid = nid;
    entry->valid = 1;
    return &entry->node;
}

void node_invalidate(f2fs_info_t *info, nid_t nid) {
    if (!info || !info->buffer_pool) return;
    
    struct f2fs_buffer_pool *pool = info->buffer_pool;
    uint32_t idx = hash_nid(nid);
    node_cache_t *entry = &pool->node_cache[idx];
    
    if (entry->valid && entry->nid == nid) {
        entry->valid = 0;
    }
}

struct f2fs_dentry_block* dentry_get(f2fs_info_t *info, block_t blkaddr) {
    if (!info || !info->buffer_pool) return NULL;
    
    struct f2fs_buffer_pool *pool = info->buffer_pool;
    
    if (pool->dentry_cache.valid && pool->dentry_cache.blkaddr == blkaddr) {
        return &pool->dentry_cache.block;
    }
    
    if (dev_read_block(info, &pool->dentry_cache.block, blkaddr) != 0) {
        return NULL;
    }
    
    pool->dentry_cache.blkaddr = blkaddr;
    pool->dentry_cache.valid = 1;
    return &pool->dentry_cache.block;
}

void dentry_invalidate(f2fs_info_t *info) {
    if (!info || !info->buffer_pool) return;
    ((struct f2fs_buffer_pool *)info->buffer_pool)->dentry_cache.valid = 0;
}

void* large_buf_get(f2fs_info_t *info) {
    if (!info || !info->buffer_pool) return NULL;
    return ((struct f2fs_buffer_pool *)info->buffer_pool)->large_buf;
}
