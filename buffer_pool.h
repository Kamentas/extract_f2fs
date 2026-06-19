#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

typedef struct f2fs_buffer_pool f2fs_buffer_pool_t;

int bufpool_init(f2fs_info_t *info);
void bufpool_free(f2fs_info_t *info);
void bufpool_reset(f2fs_info_t *info);

struct f2fs_node* node_get(f2fs_info_t *info, uint32_t nid);
void node_invalidate(f2fs_info_t *info, uint32_t nid);

struct f2fs_dentry_block* dentry_get(f2fs_info_t *info, uint32_t blkaddr);
void dentry_invalidate(f2fs_info_t *info);

void* large_buf_get(f2fs_info_t *info);
