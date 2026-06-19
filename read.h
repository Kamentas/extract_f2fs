#pragma once

#include "structs.h"
#include <stdio.h>
#include <stdbool.h>

typedef struct sparse_ctx sparse_ctx_t;
struct f2fs_buffer_pool;
typedef struct f2fs_info f2fs_info_t;
struct f2fs_info {
    FILE *fd;
    struct sparse_ctx *sparse_ctx;
    bool is_sparse;
    struct f2fs_super_block *sb;
    struct f2fs_checkpoint *cp;
    struct f2fs_buffer_pool *buffer_pool;
    u32 segment0_blkaddr;
    u32 main_blkaddr;
    u32 cp_blkaddr;
    u32 sit_blkaddr;
    u32 nat_blkaddr;
    u32 ssa_blkaddr;
    u32 log_blocksize;
    u32 log_blocks_per_seg;
    u32 blocks_per_seg;
    u32 segs_per_sec;
    u32 secs_per_zone;
    u8 *work_buffer;
    u32 work_buffer_size;
    int cur_cp;
    u64 total_valid_block_count;
    u32 total_valid_node_count;
    u32 total_valid_inode_count;
};

f2fs_info_t* f2fs_init(const char *device_path);
void f2fs_cleanup(f2fs_info_t *info);

int read_superblock(f2fs_info_t *info);
int read_checkpoint(f2fs_info_t *info);

void print_info(f2fs_info_t *info);

int get_nat_entry(f2fs_info_t *info, nid_t nid, struct f2fs_nat_entry *ne);
block_t get_node_blkaddr(f2fs_info_t *info, nid_t nid);

int read_node_block(f2fs_info_t *info, block_t blkaddr, struct f2fs_node *node);
int read_inode(f2fs_info_t *info, nid_t ino, struct f2fs_node *inode);

int read_data_block(f2fs_info_t *info, block_t blkaddr, void *buf);

int dev_read_block(f2fs_info_t *info, void *buf, block_t blkaddr);
int dev_read_blocks(f2fs_info_t *info, void *buf, block_t blkaddr, u32 count);

u32 get_extra_isize(struct f2fs_inode *inode);
bool is_inode_node(struct f2fs_node *node);

static inline int test_bit_le(u32 nr, const u8 *addr) {
    return ((1 << (nr & 7)) & (addr[nr >> 3]));
}
