#include "read.h"
#include <stdlib.h>
#include <string.h>
#include <errno.h>

bool sparse_detect(const char *path);
struct sparse_ctx *sparse_open(const char *path);
void sparse_close(struct sparse_ctx *ctx);
int sparse_read_block(struct sparse_ctx *ctx, uint32_t blkaddr, void *buf);
ssize_t sparse_read(struct sparse_ctx *ctx, uint64_t offset, void *buf, size_t size);

int bufpool_init(f2fs_info_t *info);
void bufpool_free(f2fs_info_t *info);
struct f2fs_node* node_get(f2fs_info_t *info, nid_t nid);

int dev_read_block(f2fs_info_t *info, void *buf, block_t blkaddr) {
    if (info->is_sparse && info->sparse_ctx) {
        return sparse_read_block(info->sparse_ctx, blkaddr, buf);
    }

    if (fseek(info->fd, (off_t)blkaddr * F2FS_BLKSIZE, SEEK_SET) != 0) {
        return -1;
    }
    size_t read_size = fread(buf, 1, F2FS_BLKSIZE, info->fd);
    return (read_size == F2FS_BLKSIZE) ? 0 : -1;
}

int dev_read_blocks(f2fs_info_t *info, void *buf, block_t blkaddr, u32 count) {
    if (info->is_sparse && info->sparse_ctx) {
        uint64_t offset = (uint64_t)blkaddr * F2FS_BLKSIZE;
        size_t total_size = (size_t)count * F2FS_BLKSIZE;
        ssize_t ret = sparse_read(info->sparse_ctx, offset, buf, total_size);
        return (ret == (ssize_t)total_size) ? 0 : -1;
    }

    if (fseek(info->fd, (off_t)blkaddr * F2FS_BLKSIZE, SEEK_SET) != 0) {
        return -1;
    }
    size_t total_size = (size_t)count * F2FS_BLKSIZE;
    size_t read_size = fread(buf, 1, total_size, info->fd);
    return (read_size == total_size) ? 0 : -1;
}

static int check_sb(struct f2fs_super_block *sb) {
    if (le32_to_cpu(sb->magic) != F2FS_SUPER_MAGIC) return -1;
    if (le32_to_cpu(sb->log_blocksize) != 12) return -1;
    return 0;
}

int read_superblock(f2fs_info_t *info) {
    u8 *buf = malloc(F2FS_BLKSIZE * 2);
    if (!buf) return -1;

    ssize_t bytes_read;

    if (info->is_sparse && info->sparse_ctx) {
        bytes_read = sparse_read(info->sparse_ctx, F2FS_SUPER_OFFSET,
                                 buf, sizeof(struct f2fs_super_block));
    } else {
        if (fseek(info->fd, F2FS_SUPER_OFFSET, SEEK_SET) != 0) {
            free(buf);
            return -1;
        }
        bytes_read = fread(buf, 1, sizeof(struct f2fs_super_block), info->fd);
    }

    if (bytes_read != sizeof(struct f2fs_super_block)) {
        free(buf);
        return -1;
    }

    info->sb = (struct f2fs_super_block *)buf;

    if (check_sb(info->sb) != 0) {
        if (info->is_sparse && info->sparse_ctx) {
            bytes_read = sparse_read(info->sparse_ctx,
                                     F2FS_SUPER_OFFSET + F2FS_BLKSIZE,
                                     buf, sizeof(struct f2fs_super_block));
        } else {
            if (fseek(info->fd, F2FS_SUPER_OFFSET + F2FS_BLKSIZE, SEEK_SET) != 0) {
                free(buf);
                return -1;
            }
            bytes_read = fread(buf, 1, sizeof(struct f2fs_super_block), info->fd);
        }

        if (bytes_read != sizeof(struct f2fs_super_block)) {
            free(buf);
            return -1;
        }

        if (check_sb(info->sb) != 0) {
            free(buf);
            return -1;
        }
    }
    info->segment0_blkaddr = le32_to_cpu(info->sb->segment0_blkaddr);
    info->cp_blkaddr = le32_to_cpu(info->sb->cp_blkaddr);
    info->sit_blkaddr = le32_to_cpu(info->sb->sit_blkaddr);
    info->nat_blkaddr = le32_to_cpu(info->sb->nat_blkaddr);
    info->ssa_blkaddr = le32_to_cpu(info->sb->ssa_blkaddr);
    info->main_blkaddr = le32_to_cpu(info->sb->main_blkaddr);
    info->log_blocksize = le32_to_cpu(info->sb->log_blocksize);
    info->log_blocks_per_seg = le32_to_cpu(info->sb->log_blocks_per_seg);
    info->blocks_per_seg = 1 << info->log_blocks_per_seg;
    info->segs_per_sec = le32_to_cpu(info->sb->segs_per_sec);
    info->secs_per_zone = le32_to_cpu(info->sb->secs_per_zone);

    return 0;
}

int read_checkpoint(f2fs_info_t *info) {
    u8 *buf = malloc(F2FS_BLKSIZE * 16);
    if (!buf) return -1;

    block_t cp_addr = info->cp_blkaddr;
    if (dev_read_block(info, buf, cp_addr) != 0) {
        free(buf);
        return -1;
    }

    struct f2fs_checkpoint *cp1 = (struct f2fs_checkpoint *)buf;
    u64 cp1_ver = le64_to_cpu(cp1->checkpoint_ver);

    block_t cp2_addr = cp_addr + info->blocks_per_seg;
    u8 *buf2 = malloc(F2FS_BLKSIZE * 16);
    if (!buf2) {
        info->cp = cp1;
        info->cur_cp = 1;
        return 0;
    }

    if (dev_read_block(info, buf2, cp2_addr) != 0) {
        info->cp = cp1;
        info->cur_cp = 1;
        free(buf2);
        return 0;
    }

    struct f2fs_checkpoint *cp2 = (struct f2fs_checkpoint *)buf2;
    u64 cp2_ver = le64_to_cpu(cp2->checkpoint_ver);

    if (cp2_ver > cp1_ver) {
        info->cp = cp2;
        info->cur_cp = 2;
        free(buf);
    } else {
        info->cp = cp1;
        info->cur_cp = 1;
        free(buf2);
    }

    if (le64_to_cpu(info->cp->checkpoint_ver) == 0) {
        return -1;
    }

    info->total_valid_block_count = le64_to_cpu(info->cp->valid_block_count);
    info->total_valid_node_count = le32_to_cpu(info->cp->valid_node_count);
    info->total_valid_inode_count = le32_to_cpu(info->cp->valid_inode_count);

    return 0;
}

block_t get_nat_addr(f2fs_info_t *info, nid_t nid) {
    return info->nat_blkaddr + nid / NAT_ENTRY_PER_BLOCK;
}

int get_nat_entry(f2fs_info_t *info, nid_t nid, struct f2fs_nat_entry *ne) {
    if (!ne || !info) return -1;

    block_t addr = get_nat_addr(info, nid);
    struct f2fs_nat_block *blk = malloc(sizeof(*blk));
    if (!blk) return -1;

    if (dev_read_block(info, blk, addr) != 0) {
        free(blk);
        return -1;
    }

    memcpy(ne, &blk->entries[nid % NAT_ENTRY_PER_BLOCK], sizeof(*ne));
    free(blk);
    return 0;
}

block_t get_node_blkaddr(f2fs_info_t *info, nid_t nid) {
    struct f2fs_nat_entry ne;
    if (get_nat_entry(info, nid, &ne) != 0) {
        return NULL_ADDR;
    }
    return le32_to_cpu(ne.block_addr);
}

int read_node_block(f2fs_info_t *info, block_t blkaddr, struct f2fs_node *node) {
    block_t main = info->main_blkaddr;
    if (blkaddr == NULL_ADDR || blkaddr == NEW_ADDR || blkaddr == COMPRESS_ADDR) {
        return -1;
    }
    if (blkaddr < main) return -1;
    if (blkaddr >= le64_to_cpu(info->sb->block_count)) return -1;
    return dev_read_block(info, node, blkaddr);
}

int read_inode(f2fs_info_t *info, nid_t ino, struct f2fs_node *inode) {
    struct f2fs_node *cached = node_get(info, ino);
    if (cached) {
        memcpy(inode, cached, sizeof(struct f2fs_node));
        return 0;
    }

    block_t blkaddr = get_node_blkaddr(info, ino);
    if (blkaddr == NULL_ADDR) {
        return -1;
    }
    return read_node_block(info, blkaddr, inode);
}

int read_data_block(f2fs_info_t *info, block_t blkaddr, void *buf) {
    block_t main = info->main_blkaddr;
    if (blkaddr == NULL_ADDR) {
        memset(buf, 0, F2FS_BLKSIZE);
        return 0;
    }
    if (blkaddr == NEW_ADDR || blkaddr == COMPRESS_ADDR) {
        return -1;
    }
    if (blkaddr < main) return -1;
    if (blkaddr >= le64_to_cpu(info->sb->block_count)) return -1;
    return dev_read_block(info, buf, blkaddr);
}

u32 get_extra_isize(struct f2fs_inode *inode) {
    return (inode->i_inline & F2FS_EXTRA_ATTR) ? le16_to_cpu(inode->i_extra_isize) / 4 : 0;
}

bool is_inode_node(struct f2fs_node *node) {
    struct node_footer *footer = F2FS_NODE_FOOTER(node);
    return le32_to_cpu(footer->nid) == le32_to_cpu(footer->ino);
}

void print_info(f2fs_info_t *info) {
    struct f2fs_super_block *sb = info->sb;
    struct f2fs_checkpoint *cp = info->cp;
    printf("Superblock Info:\n");
    printf("  Magic: 0x%x\n", le32_to_cpu(sb->magic));
    printf("  Version: %u.%u\n", le16_to_cpu(sb->major_ver), le16_to_cpu(sb->minor_ver));
    printf("  Block size: %u bytes\n", 1 << le32_to_cpu(sb->log_blocksize));
    printf("  Blocks per segment: %u\n", 1 << le32_to_cpu(sb->log_blocks_per_seg));
    printf("  Segments per section: %u\n", le32_to_cpu(sb->segs_per_sec));
    printf("  Sections per zone: %u\n", le32_to_cpu(sb->secs_per_zone));
    printf("  Total blocks: %llu\n", (unsigned long long)le64_to_cpu(sb->block_count));
    printf("  Total segments: %u\n", le32_to_cpu(sb->segment_count));
    printf("  Main area segments: %u\n", le32_to_cpu(sb->segment_count_main));
    printf("  Root inode: %u\n", le32_to_cpu(sb->root_ino));
    printf("  CP blkaddr: %u\n", le32_to_cpu(sb->cp_blkaddr));
    printf("  NAT blkaddr: %u\n", le32_to_cpu(sb->nat_blkaddr));
    printf("  SIT blkaddr: %u\n", le32_to_cpu(sb->sit_blkaddr));
    printf("  SSA blkaddr: %u\n", le32_to_cpu(sb->ssa_blkaddr));
    printf("  Main blkaddr: %u\n\n", le32_to_cpu(sb->main_blkaddr));
    printf("Checkpoint Info:\n");
    printf("  Checkpoint version: %llu\n", (unsigned long long)le64_to_cpu(cp->checkpoint_ver));
    printf("  User block count: %llu\n", (unsigned long long)le64_to_cpu(cp->user_block_count));
    printf("  Valid block count: %llu\n", (unsigned long long)le64_to_cpu(cp->valid_block_count));
    printf("  Valid node count: %u\n", le32_to_cpu(cp->valid_node_count));
    printf("  Valid inode count: %u\n", le32_to_cpu(cp->valid_inode_count));
    printf("  Free segments: %u\n", le32_to_cpu(cp->free_segment_count));
    printf("  Checkpoint flags: 0x%x\n", le32_to_cpu(cp->ckpt_flags));
}

f2fs_info_t* f2fs_init(const char *device_path) {
    f2fs_info_t *info = calloc(1, sizeof(f2fs_info_t));
    if (!info) return NULL;

    if (sparse_detect(device_path)) {
        info->is_sparse = true;
        struct sparse_ctx *sparse_ctx = sparse_open(device_path);
        if (!sparse_ctx) {
            free(info);
            return NULL;
        }
        info->sparse_ctx = sparse_ctx;
        info->fd = NULL;
    } else {
        info->is_sparse = false;
        info->fd = fopen(device_path, "rb");
        if (!info->fd) {
            fprintf(stderr, "Error: Failed to open %s: %s\n",
                    device_path, strerror(errno));
            free(info);
            return NULL;
        }
    }

    if (read_superblock(info) != 0) {
        if (info->is_sparse && info->sparse_ctx) {
            sparse_close(info->sparse_ctx);
        } else if (info->fd) {
            fclose(info->fd);
        }
        free(info);
        return NULL;
    }

    if (read_checkpoint(info) != 0) {
        free(info->sb);
        if (info->is_sparse && info->sparse_ctx) {
            sparse_close(info->sparse_ctx);
        } else if (info->fd) {
            fclose(info->fd);
        }
        free(info);
        return NULL;
    }

    if (bufpool_init(info) != 0) {
        free(info->cp);
        free(info->sb);
        if (info->is_sparse && info->sparse_ctx) {
            sparse_close(info->sparse_ctx);
        } else if (info->fd) {
            fclose(info->fd);
        }
        free(info);
        return NULL;
    }

    return info;
}

void f2fs_cleanup(f2fs_info_t *info) {
    if (!info) return;

    if (info->is_sparse && info->sparse_ctx) {
        sparse_close(info->sparse_ctx);
    } else if (info->fd) {
        fclose(info->fd);
    }

    free(info->sb);
    free(info->cp);
    bufpool_free(info);
    free(info);
}
