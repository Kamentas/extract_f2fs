#pragma once

#include "read.h"
#include "buffer_pool.h"
#include <stdbool.h>

typedef struct {
    char name[F2FS_NAME_LEN + 1];
    nid_t ino;
    u32 parent_ino;
    u8 file_type;
    u64 size;
    u32 mode;
    u32 uid;
    u32 gid;
    u64 atime;
    u64 ctime;
    u64 mtime;
    u32 links;
    bool is_compressed;
} file_info_t;

static const uint32_t F2FS_XATTR_MAGIC = 0xF2F52011;
static const uint32_t XATTR_ROUND = 3;

struct f2fs_xattr_header {
    u32 h_magic;
    u32 h_refcount;
    u32 h_sloadd[4];
} __attribute__((packed));

struct f2fs_xattr_entry {
    u8 e_name_index;
    u8 e_name_len;
    u16 e_value_size;
    char e_name[0];
} __attribute__((packed));

static inline struct f2fs_xattr_header *xattr_hdr(void *ptr) {
    return (struct f2fs_xattr_header *)ptr;
}

static inline struct f2fs_xattr_entry *xattr_entry(void *ptr) {
    return (struct f2fs_xattr_entry *)ptr;
}

static inline size_t entry_size(const struct f2fs_xattr_entry *entry) {
    return (((size_t)sizeof(struct f2fs_xattr_entry) +
             entry->e_name_len +
             le16_to_cpu(entry->e_value_size) + XATTR_ROUND) & ~XATTR_ROUND);
}

static inline struct f2fs_xattr_entry *xattr_first_entry(void *ptr) {
    return xattr_entry(xattr_hdr(ptr) + 1);
}

static inline bool is_xattr_last_entry(const struct f2fs_xattr_entry *entry) {
    return *((const u32 *)entry) == 0;
}

static inline struct f2fs_xattr_entry *xattr_next_entry(const struct f2fs_xattr_entry *entry) {
    return (struct f2fs_xattr_entry *)((const char *)entry + entry_size(entry));
}

#define F2FS_XATTR_INDEX_USER               1
#define F2FS_XATTR_INDEX_POSIX_ACL_ACCESS   2
#define F2FS_XATTR_INDEX_POSIX_ACL_DEFAULT  3
#define F2FS_XATTR_INDEX_TRUSTED            4
#define F2FS_XATTR_INDEX_LUSTRE             5
#define F2FS_XATTR_INDEX_SECURITY           6
#define F2FS_XATTR_INDEX_ENCRYPTION         9
#define F2FS_XATTR_INDEX_VERITY             11

typedef struct {
    char name[F2FS_NAME_LEN + 1];
    u8 *value;
    u32 value_len;
} xattr_info_t;

typedef struct {
    xattr_info_t *xattrs;
    u32 count;
} xattr_list_t;

typedef int (*dir_callback_t)(f2fs_info_t *info, file_info_t *file, void *user_data);

int extract_file(f2fs_info_t *info, nid_t ino, const char *out_path);
int extract_directory(f2fs_info_t *info, nid_t ino, const char *out_path);
int extract_symlink(f2fs_info_t *info, nid_t ino, const char *out_path);
int extract_tree(f2fs_info_t *info, nid_t ino, const char *out_path);

xattr_list_t *read_xattrs_from_inode(f2fs_info_t *info, struct f2fs_node *inode);
void free_xattr_list(xattr_list_t *xattr_list);

int read_file_data(f2fs_info_t *info, struct f2fs_node *inode, 
                   u8 *buffer, u64 count, u64 offset);

int list_directory(f2fs_info_t *info, nid_t dir_ino, 
                   dir_callback_t callback, void *user_data);
int find_entry_in_dir(f2fs_info_t *info, nid_t dir_ino, 
                      const char *name, nid_t *found_ino);

int lookup_path(f2fs_info_t *info, const char *path, nid_t *result_ino);

int get_data_blkaddr(f2fs_info_t *info, struct f2fs_node *inode,
                     u64 file_block, block_t *blkaddr);

int get_file_info(f2fs_info_t *info, nid_t ino, file_info_t *file_info);

int sanitize_filename(const char *input, char *output, size_t out_size);
