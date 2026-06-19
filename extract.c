#include "extract.h"
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <libgen.h>
#include <ctype.h>

#ifdef _WIN32
#include <direct.h>
#define mkdir_p(path) _mkdir(path)
#else
#define mkdir_p(path) mkdir(path, 0755)
#endif

#define DIR1_BLOCK (DEF_ADDRS_PER_INODE + 1)
#define DIR2_BLOCK (DEF_ADDRS_PER_INODE + 2)
#define IND1_BLOCK (DEF_ADDRS_PER_INODE + 3)
#define IND2_BLOCK (DEF_ADDRS_PER_INODE + 4)
#define DIND_BLOCK (DEF_ADDRS_PER_INODE + 5)

static inline bool is_inline_data(struct f2fs_inode *inode) {
    return (inode->i_inline & F2FS_INLINE_DATA) != 0;
}

static inline bool is_inline_dentry(struct f2fs_inode *inode) {
    return (inode->i_inline & F2FS_INLINE_DENTRY) != 0;
}

static inline bool is_compressed(struct f2fs_inode *inode) {
    return (le32_to_cpu(inode->i_flags) & F2FS_COMPR_FL) != 0;
}

static int read_inline(struct f2fs_node *inode, u8 *buffer, u64 *size) {
    u32 extra = get_extra_isize(&inode->i);
    u32 start = extra;
    u32 max = DEF_ADDRS_PER_INODE - start;
    
    if (inode->i.i_inline & F2FS_INLINE_XATTR) {
        u32 xsz = DEFAULT_INLINE_XATTR_ADDRS;
        if (inode->i.i_inline & F2FS_EXTRA_ATTR) {
            xsz = le16_to_cpu(inode->i.i_inline_xattr_size);
        }
        max -= xsz;
    }
    
    u64 fsize = le64_to_cpu(inode->i.i_size);
    u64 copy = (fsize < max * 4) ? fsize : max * 4;
    
    memcpy(buffer, &inode->i.i_addr[start], copy);
    *size = copy;
    return 0;
}

static int node_path(struct f2fs_inode *inode, u64 idx, u32 off[4], u32 noff[4]) {
    u32 direct_idx = DEF_ADDRS_PER_INODE;
    u32 direct_blks = DEF_ADDRS_PER_BLOCK;
    u32 indirect_blks = DEF_ADDRS_PER_BLOCK * NIDS_PER_BLOCK;
    u32 dindirect_blks = indirect_blks * NIDS_PER_BLOCK;
    int n = 0, level = 0;
    
    noff[0] = 0;
    
    if (idx < direct_idx) {
        off[n] = idx;
        goto done;
    }
    
    idx -= direct_idx;
    if (idx < direct_blks) {
        off[n++] = DIR1_BLOCK;
        noff[n] = 1;
        off[n] = idx;
        level = 1;
        goto done;
    }
    
    idx -= direct_blks;
    if (idx < direct_blks) {
        off[n++] = DIR2_BLOCK;
        noff[n] = 2;
        off[n] = idx;
        level = 1;
        goto done;
    }
    
    idx -= direct_blks;
    if (idx < indirect_blks) {
        off[n++] = IND1_BLOCK;
        noff[n] = 3;
        off[n++] = idx / direct_blks;
        noff[n] = 4 + off[n - 1];
        off[n] = idx % direct_blks;
        level = 2;
        goto done;
    }
    
    idx -= indirect_blks;
    if (idx < indirect_blks) {
        off[n++] = IND2_BLOCK;
        noff[n] = 4 + NIDS_PER_BLOCK;
        off[n++] = idx / direct_blks;
        noff[n] = 5 + NIDS_PER_BLOCK + off[n - 1];
        off[n] = idx % direct_blks;
        level = 2;
        goto done;
    }
    
    idx -= indirect_blks;
    if (idx < dindirect_blks) {
        off[n++] = DIND_BLOCK;
        noff[n] = 5 + (NIDS_PER_BLOCK * 2);
        off[n++] = idx / indirect_blks;
        noff[n] = 6 + (NIDS_PER_BLOCK * 2) + off[n - 1] * (NIDS_PER_BLOCK + 1);
        off[n++] = (idx / direct_blks) % NIDS_PER_BLOCK;
        noff[n] = 7 + (NIDS_PER_BLOCK * 2) + off[n - 2] * (NIDS_PER_BLOCK + 1) + off[n - 1];
        off[n] = idx % direct_blks;
        level = 3;
        goto done;
    }
    
    return -1;
    
done:
    return level;
}

int get_data_blkaddr(f2fs_info_t *info, struct f2fs_node *inode, u64 file_block, block_t *blkaddr) {
    u32 offset[4], noffset[4];
    struct f2fs_node *node_blk = NULL;
    nid_t nid;
    int level, i;
    u32 extra = get_extra_isize(&inode->i);
    
    level = node_path(&inode->i, file_block, offset, noffset);
    if (level < 0) return -1;
    
    if (level == 0) {
        *blkaddr = le32_to_cpu(inode->i.i_addr[offset[0] + extra]);
        return 0;
    }
    
    node_blk = malloc(sizeof(*node_blk));
    if (!node_blk) return -1;
    
    nid = le32_to_cpu(inode->i.i_addr[offset[0] + extra]);
    
    for (i = 1; i <= level; i++) {
        if (nid == 0) {
            *blkaddr = NULL_ADDR;
            free(node_blk);
            return 0;
        }
        
        block_t addr = get_node_blkaddr(info, nid);
        if (addr == NULL_ADDR || read_node_block(info, addr, node_blk) != 0) {
            free(node_blk);
            return -1;
        }
        
        if (i == level) {
            *blkaddr = le32_to_cpu(node_blk->dn.addr[offset[i]]);
        } else {
            nid = le32_to_cpu(node_blk->in.nid[offset[i]]);
        }
    }
    
    free(node_blk);
    return 0;
}

int read_file_data(f2fs_info_t *info, struct f2fs_node *inode, u8 *buffer, u64 count, u64 offset) {
    u64 fsize = le64_to_cpu(inode->i.i_size);
    
    if (is_inline_data(&inode->i)) {
        u64 isize;
        if (read_inline(inode, buffer, &isize) != 0) return -1;
        
        if (offset >= isize) return 0;
        
        u64 rsize = (offset + count > isize) ? (isize - offset) : count;
        memmove(buffer, buffer + offset, rsize);
        return rsize;
    }
    
    if (offset >= fsize) return 0;
    if (offset + count > fsize) count = fsize - offset;
    
    u64 read_cnt = 0;
    u8 *blk_buf = malloc(F2FS_BLKSIZE);
    if (!blk_buf) return -1;
    
    while (count > 0) {
        u64 fb = (offset + read_cnt) / F2FS_BLKSIZE;
        u64 boff = (offset + read_cnt) % F2FS_BLKSIZE;
        u64 brem = F2FS_BLKSIZE - boff;
        u64 to_read = (count < brem) ? count : brem;
        
        block_t blkaddr;
        if (get_data_blkaddr(info, inode, fb, &blkaddr) != 0 ||
            read_data_block(info, blkaddr, blk_buf) != 0) {
            free(blk_buf);
            return -1;
        }
        
        memcpy(buffer + read_cnt, blk_buf + boff, to_read);
        read_cnt += to_read;
        count -= to_read;
    }
    
    free(blk_buf);
    return read_cnt;
}

int read_dentry_block(f2fs_info_t *info, block_t blkaddr, dir_callback_t cb, void *ud) {
    struct f2fs_dentry_block *blk = malloc(sizeof(*blk));
    if (!blk || dev_read_block(info, blk, blkaddr) != 0) {
        free(blk);
        return -1;
    }
    
    for (int i = 0; i < NR_DENTRY_IN_BLOCK; i++) {
        if (!test_bit_le(i, blk->dentry_bitmap)) continue;
        
        struct f2fs_dir_entry *de = &blk->dentry[i];
        u16 nlen = le16_to_cpu(de->name_len);
        
        if (nlen == 0 || nlen > F2FS_NAME_LEN) continue;
        
        file_info_t f;
        memset(&f, 0, sizeof(f));
        
        memcpy(f.name, &blk->filename[i][0], nlen);
        f.name[nlen] = '\0';
        f.ino = le32_to_cpu(de->ino);
        f.file_type = de->file_type;
        
        if (strcmp(f.name, ".") == 0 || strcmp(f.name, "..") == 0) continue;
        if (cb && cb(info, &f, ud) != 0) {
            free(blk);
            return -1;
        }
    }
    
    free(blk);
    return 0;
}

int read_inline_dentry(f2fs_info_t *info, struct f2fs_node *inode, dir_callback_t cb, void *ud) {
    u32 extra = get_extra_isize(&inode->i);
    u8 *dentry = (u8 *)&inode->i.i_addr[extra];
    
    u32 max = DEF_ADDRS_PER_INODE - extra;
    if (inode->i.i_inline & F2FS_INLINE_XATTR) {
        u32 xsz = DEFAULT_INLINE_XATTR_ADDRS;
        if (inode->i.i_inline & F2FS_EXTRA_ATTR) {
            xsz = le16_to_cpu(inode->i.i_inline_xattr_size);
        }
        max -= xsz;
    }
    
    u32 nr = (max * 32) / ((SIZE_OF_DIR_ENTRY + F2FS_SLOT_LEN) * 8 + 1);
    u32 bm_sz = (nr + 7) / 8;
    
    u8 *bm = dentry;
    struct f2fs_dir_entry *des = (struct f2fs_dir_entry *)(bm + bm_sz + 1);
    u8 *names = (u8 *)(des + nr);
    
    for (u32 i = 0; i < nr; i++) {
        if (!test_bit_le(i, bm)) continue;
        
        struct f2fs_dir_entry *de = &des[i];
        u16 nlen = le16_to_cpu(de->name_len);
        
        if (nlen == 0 || nlen > F2FS_NAME_LEN) continue;
        
        file_info_t f;
        memset(&f, 0, sizeof(f));
        
        memcpy(f.name, names + i * F2FS_SLOT_LEN, nlen);
        f.name[nlen] = '\0';
        f.ino = le32_to_cpu(de->ino);
        f.file_type = de->file_type;
        
        if (strcmp(f.name, ".") == 0 || strcmp(f.name, "..") == 0) continue;
        if (cb && cb(info, &f, ud) != 0) return -1;
    }
    
    return 0;
}

int list_directory(f2fs_info_t *info, nid_t dir_ino, dir_callback_t cb, void *ud) {
    struct f2fs_node *inode = malloc(sizeof(*inode));
    if (!inode || read_inode(info, dir_ino, inode) != 0) {
        free(inode);
        return -1;
    }
    
    u16 mode = le16_to_cpu(inode->i.i_mode);
    if ((mode & 0170000) != 0040000) {
        free(inode);
        return -1;
    }
    
    int ret = 0;
    if (is_inline_dentry(&inode->i)) {
        ret = read_inline_dentry(info, inode, cb, ud);
    } else {
        u64 dsize = le64_to_cpu(inode->i.i_size);
        u64 nblks = (dsize + F2FS_BLKSIZE - 1) / F2FS_BLKSIZE;
        
        for (u64 i = 0; i < nblks && ret == 0; i++) {
            block_t blkaddr;
            if (get_data_blkaddr(info, inode, i, &blkaddr) == 0 &&
                (blkaddr == NULL_ADDR || blkaddr == NEW_ADDR ||
                 read_dentry_block(info, blkaddr, cb, ud) == 0)) {
                continue;
            }
            ret = -1;
        }
    }
    
    free(inode);
    return ret;
}

typedef struct {
    const char *name;
    nid_t *ino;
    bool found;
} find_ctx_t;

static int find_cb(f2fs_info_t *info, file_info_t *f, void *ud) {
    (void)info;
    find_ctx_t *ctx = (find_ctx_t *)ud;
    
    if (strcmp(f->name, ctx->name) == 0) {
        *ctx->ino = f->ino;
        ctx->found = true;
        return 1;
    }
    return 0;
}

int find_entry_in_dir(f2fs_info_t *info, nid_t dir_ino, const char *name, nid_t *found_ino) {
    find_ctx_t ctx = { .name = name, .ino = found_ino, .found = false };
    list_directory(info, dir_ino, find_cb, &ctx);
    return ctx.found ? 0 : -1;
}

int lookup_path(f2fs_info_t *info, const char *path, nid_t *ino) {
    if (!path || path[0] == '\0') return -1;
    
    nid_t cur = le32_to_cpu(info->sb->root_ino);
    
    if (strcmp(path, "/") == 0) {
        *ino = cur;
        return 0;
    }
    
    char *pc = strdup(path);
    if (!pc) return -1;
    
    char *p = pc;
    if (*p == '/') p++;
    
    char *saveptr;
    char *comp = strtok_r(p, "/", &saveptr);
    
    while (comp) {
        if (find_entry_in_dir(info, cur, comp, &cur) != 0) {
            free(pc);
            return -1;
        }
        comp = strtok_r(NULL, "/", &saveptr);
    }
    
    *ino = cur;
    free(pc);
    return 0;
}

int get_file_info(f2fs_info_t *info, nid_t ino, file_info_t *fi) {
    struct f2fs_node *inode = malloc(sizeof(*inode));
    if (!inode || read_inode(info, ino, inode) != 0) {
        free(inode);
        return -1;
    }
    
    fi->ino = ino;
    fi->size = le64_to_cpu(inode->i.i_size);
    fi->mode = le16_to_cpu(inode->i.i_mode);
    fi->uid = le32_to_cpu(inode->i.i_uid);
    fi->gid = le32_to_cpu(inode->i.i_gid);
    fi->atime = le64_to_cpu(inode->i.i_atime);
    fi->ctime = le64_to_cpu(inode->i.i_ctime);
    fi->mtime = le64_to_cpu(inode->i.i_mtime);
    fi->links = le32_to_cpu(inode->i.i_links);
    fi->is_compressed = is_compressed(&inode->i);
    fi->parent_ino = le32_to_cpu(inode->i.i_pino);
    
    u16 m = fi->mode;
    if ((m & 0170000) == 0100000) fi->file_type = F2FS_FT_REG_FILE;
    else if ((m & 0170000) == 0040000) fi->file_type = F2FS_FT_DIR;
    else if ((m & 0170000) == 0120000) fi->file_type = F2FS_FT_SYMLINK;
    else if ((m & 0170000) == 0020000) fi->file_type = F2FS_FT_CHRDEV;
    else if ((m & 0170000) == 0060000) fi->file_type = F2FS_FT_BLKDEV;
    else if ((m & 0170000) == 0010000) fi->file_type = F2FS_FT_FIFO;
    else if ((m & 0170000) == 0140000) fi->file_type = F2FS_FT_SOCK;
    else fi->file_type = F2FS_FT_UNKNOWN;
    
    u32 nlen = le32_to_cpu(inode->i.i_namelen);
    if (nlen > 0 && nlen <= F2FS_NAME_LEN) {
        memcpy(fi->name, inode->i.i_name, nlen);
        fi->name[nlen] = '\0';
    } else {
        fi->name[0] = '\0';
    }
    
    free(inode);
    return 0;
}

static int mk_parent_dir(const char *path) {
    char *pc = strdup(path);
    if (!pc) return -1;
    
    char *dir = dirname(pc);
    
    struct stat st;
    int ret = 0;
    if (stat(dir, &st) != 0) {
        ret = (mkdir_p(dir) != 0 && errno != EEXIST) ? -1 : 0;
    }
    
    free(pc);
    return ret;
}

int sanitize_filename(const char *in, char *out, size_t sz) {
    size_t i, j = 0;
    
    for (i = 0; i < strlen(in) && j < sz - 1; i++) {
        char c = in[i];
        
        if (c == '/' || c == '\0' || c == '\\' || !isprint(c)) {
            out[j++] = '_';
        } else {
            out[j++] = c;
        }
    }
    
    out[j] = '\0';
    return 0;
}

int extract_file(f2fs_info_t *info, nid_t ino, const char *out_path) {
    struct f2fs_node *inode = malloc(sizeof(*inode));
    if (!inode || read_inode(info, ino, inode) != 0) {
        free(inode);
        return -1;
    }
    
    u64 fsize = le64_to_cpu(inode->i.i_size);
    
    mk_parent_dir(out_path);
    
    FILE *fp = fopen(out_path, "wb");
    if (!fp) {
        free(inode);
        return -1;
    }
    
    const size_t bufsz = 1024 * 1024;
    u8 *buf = malloc(bufsz);
    if (!buf) {
        fclose(fp);
        free(inode);
        return -1;
    }
    
    u64 off = 0;
    while (off < fsize) {
        u64 to_read = (fsize - off > bufsz) ? bufsz : (fsize - off);
        
        int rsize = read_file_data(info, inode, buf, to_read, off);
        if (rsize < 0) {
            free(buf);
            fclose(fp);
            free(inode);
            return -1;
        }
        
        if (rsize == 0) break;
        
        if (fwrite(buf, 1, rsize, fp) != (size_t)rsize) {
            free(buf);
            fclose(fp);
            free(inode);
            return -1;
        }
        
        off += rsize;
    }
    
    free(buf);
    fclose(fp);
    free(inode);
    return 0;
}

int extract_symlink(f2fs_info_t *info, nid_t ino, const char *out_path) {
    struct f2fs_node *inode = malloc(sizeof(*inode));
    if (!inode || read_inode(info, ino, inode) != 0) {
        free(inode);
        return -1;
    }
    
    u64 len = le64_to_cpu(inode->i.i_size);
    u8 *target = malloc(len + 1);
    if (!target) {
        free(inode);
        return -1;
    }
    
    int rsize = read_file_data(info, inode, target, len, 0);
    if (rsize < 0) {
        free(target);
        free(inode);
        return -1;
    }
    
    target[rsize] = '\0';
    
    mk_parent_dir(out_path);
    
#ifndef _WIN32
    symlink((char *)target, out_path);
#else
    FILE *fp = fopen(out_path, "w");
    if (fp) {
        fprintf(fp, "SYMLINK: %s\n", target);
        fclose(fp);
    }
#endif
    
    free(target);
    free(inode);
    return 0;
}

typedef struct {
    f2fs_info_t *info;
    const char *base_path;
} ext_ctx_t;

static int ext_cb(f2fs_info_t *info, file_info_t *f, void *ud) {
    ext_ctx_t *ctx = (ext_ctx_t *)ud;
    
    char out_path[4096];
    char safe_name[F2FS_NAME_LEN + 1];
    
    sanitize_filename(f->name, safe_name, sizeof(safe_name));
    snprintf(out_path, sizeof(out_path), "%s/%s", ctx->base_path, safe_name);
    
    if (f->file_type == F2FS_FT_REG_FILE) {
        return extract_file(info, f->ino, out_path);
    } else if (f->file_type == F2FS_FT_DIR) {
        return extract_tree(info, f->ino, out_path);
    } else if (f->file_type == F2FS_FT_SYMLINK) {
        return extract_symlink(info, f->ino, out_path);
    }
    
    return 0;
}

int extract_directory(f2fs_info_t *info, nid_t ino, const char *out_path) {
    (void)info;
    (void)ino;
    if (mkdir_p(out_path) != 0 && errno != EEXIST) return -1;
    return 0;
}

int extract_tree(f2fs_info_t *info, nid_t ino, const char *out_path) {
    if (extract_directory(info, ino, out_path) != 0) return -1;
    
    ext_ctx_t ctx = { .info = info, .base_path = out_path };
    return list_directory(info, ino, ext_cb, &ctx);
}

static const char* xattr_name(u8 idx) {
    switch (idx) {
        case F2FS_XATTR_INDEX_USER: return "user";
        case F2FS_XATTR_INDEX_POSIX_ACL_ACCESS: return "system.posix_acl_access";
        case F2FS_XATTR_INDEX_POSIX_ACL_DEFAULT: return "system.posix_acl_default";
        case F2FS_XATTR_INDEX_TRUSTED: return "trusted";
        case F2FS_XATTR_INDEX_LUSTRE: return "lustre";
        case F2FS_XATTR_INDEX_SECURITY: return "security";
        case F2FS_XATTR_INDEX_ENCRYPTION: return "encryption";
        case F2FS_XATTR_INDEX_VERITY: return "verity";
        default: return "unknown";
    }
}

xattr_list_t* read_xattrs_from_inode(f2fs_info_t *info, struct f2fs_node *inode) {
    xattr_list_t *xl = NULL;
    u32 extra = get_extra_isize(&inode->i);

    if (inode->i.i_inline & F2FS_INLINE_XATTR) {
        u32 off = extra * 4;
        u32 sz = DEFAULT_INLINE_XATTR_ADDRS * 4;
        if (inode->i.i_inline & F2FS_EXTRA_ATTR) {
            sz = le16_to_cpu(inode->i.i_inline_xattr_size);
        }

        u8 *data = (u8 *)&inode->i + off;
        xl = calloc(1, sizeof(*xl));
        if (!xl) return NULL;

        struct f2fs_xattr_header *hdr = xattr_hdr(data);
        if (le32_to_cpu(hdr->h_magic) != F2FS_XATTR_MAGIC) {
            xl->xattrs = NULL;
            xl->count = 0;
        } else {
            struct f2fs_xattr_entry *e = xattr_first_entry(data);
            u32 cur = sizeof(struct f2fs_xattr_header);

            while (cur < sz && !is_xattr_last_entry(e)) {
                u32 nlen = e->e_name_len;
                u32 vlen = le16_to_cpu(e->e_value_size);

                xl->xattrs = realloc(xl->xattrs, (xl->count + 1) * sizeof(xattr_info_t));
                xattr_info_t *x = &xl->xattrs[xl->count];
                memset(x, 0, sizeof(*x));

                if (nlen > 0) {
                    if (e->e_name[0] != '\0') {
                        strncpy(x->name, e->e_name, nlen);
                        x->name[nlen] = '\0';
                    } else {
                        strncpy(x->name, xattr_name(e->e_name_index), F2FS_NAME_LEN);
                    }
                }

                if (vlen > 0) {
                    x->value = malloc(vlen);
                    if (x->value) {
                        memcpy(x->value, data + cur + sizeof(*e) + nlen, vlen);
                        x->value_len = vlen;
                    }
                }

                xl->count++;
                e = xattr_next_entry(e);
                cur += (u32)entry_size(e);
            }
        }
    }

    nid_t xnid = le32_to_cpu(inode->i.i_xattr_nid);
    if (xnid != 0) {
        struct f2fs_node *xnode = malloc(sizeof(*xnode));
        if (!xnode) {
            free_xattr_list(xl);
            return NULL;
        }

        block_t addr = get_node_blkaddr(info, xnid);
        if (addr == NULL_ADDR || read_node_block(info, addr, xnode) != 0) {
            free(xnode);
            free_xattr_list(xl);
            return NULL;
        }

        if (!xl) {
            xl = calloc(1, sizeof(*xl));
            if (!xl) {
                free(xnode);
                return NULL;
            }
        }

        u8 *data = (u8 *)xnode;
        struct f2fs_xattr_header *hdr = xattr_hdr(data);
        if (le32_to_cpu(hdr->h_magic) == F2FS_XATTR_MAGIC) {
            struct f2fs_xattr_entry *e = xattr_first_entry(data);
            u32 cur = sizeof(struct f2fs_xattr_header);

            while (cur < F2FS_BLKSIZE && !is_xattr_last_entry(e)) {
                u32 nlen = e->e_name_len;
                u32 vlen = le16_to_cpu(e->e_value_size);

                xl->xattrs = realloc(xl->xattrs, (xl->count + 1) * sizeof(xattr_info_t));
                xattr_info_t *x = &xl->xattrs[xl->count];
                memset(x, 0, sizeof(*x));

                if (nlen > 0) {
                    if (e->e_name[0] != '\0') {
                        strncpy(x->name, e->e_name, nlen);
                        x->name[nlen] = '\0';
                    } else {
                        strncpy(x->name, xattr_name(e->e_name_index), F2FS_NAME_LEN);
                    }
                }

                if (vlen > 0) {
                    x->value = malloc(vlen);
                    if (x->value) {
                        memcpy(x->value, data + cur + sizeof(*e) + nlen, vlen);
                        x->value_len = vlen;
                    }
                }

                xl->count++;
                e = xattr_next_entry(e);
                cur += (u32)entry_size(e);
            }
        }

        free(xnode);
    }

    return xl;
}

void free_xattr_list(xattr_list_t *xl) {
    if (xl) {
        if (xl->xattrs) {
            for (u32 i = 0; i < xl->count; i++) {
                free(xl->xattrs[i].value);
            }
            free(xl->xattrs);
        }
        free(xl);
    }
}
