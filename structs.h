#pragma once

#include <stdint.h>
#include <stdbool.h>

typedef uint64_t u64;
typedef uint32_t u32;
typedef uint16_t u16;
typedef uint8_t u8;
typedef u32 block_t;
typedef u32 nid_t;
typedef u32 f2fs_hash_t;

#if __BYTE_ORDER == __LITTLE_ENDIAN
static inline uint16_t le16_to_cpu(uint16_t x) { return x; }
static inline uint32_t le32_to_cpu(uint32_t x) { return x; }
static inline uint64_t le64_to_cpu(uint64_t x) { return x; }
static inline uint16_t cpu_to_le16(uint16_t x) { return x; }
static inline uint32_t cpu_to_le32(uint32_t x) { return x; }
static inline uint64_t cpu_to_le64(uint64_t x) { return x; }
#else
static inline uint16_t le16_to_cpu(uint16_t x) { return __builtin_bswap16(x); }
static inline uint32_t le32_to_cpu(uint32_t x) { return __builtin_bswap32(x); }
static inline uint64_t le64_to_cpu(uint64_t x) { return __builtin_bswap64(x); }
static inline uint16_t cpu_to_le16(uint16_t x) { return __builtin_bswap16(x); }
static inline uint32_t cpu_to_le32(uint32_t x) { return __builtin_bswap32(x); }
static inline uint64_t cpu_to_le64(uint64_t x) { return __builtin_bswap64(x); }
#endif

#define F2FS_SUPER_MAGIC        0xF2F52010
#define F2FS_SUPER_OFFSET       1024
#define F2FS_MIN_LOG_SECTOR_SIZE 9
#define F2FS_MAX_LOG_SECTOR_SIZE 12
#define F2FS_BLKSIZE            4096
#define F2FS_MAX_EXTENSION      64
#define F2FS_EXTENSION_LEN      8
#define F2FS_NAME_LEN           255
#define MAX_VOLUME_NAME         512
#define MAX_PATH_LEN            64
#define MAX_DEVICES             8
#define VERSION_LEN             256

#define NULL_ADDR               0x0U
#define NEW_ADDR                ((block_t)-1U)
#define COMPRESS_ADDR           ((block_t)-2U)

#define CP_UMOUNT_FLAG          0x00000001
#define CP_ORPHAN_PRESENT_FLAG  0x00000002
#define CP_COMPACT_SUM_FLAG     0x00000004
#define CP_ERROR_FLAG           0x00000008
#define CP_DISABLED_FLAG        0x00001000
#define CP_LARGE_NAT_BITMAP_FLAG 0x00000400
#define CP_CRC_RECOVERY_FLAG    0x00000040
#define CP_FASTBOOT_FLAG        0x00000020
#define CP_FSCK_FLAG            0x00000010

#define F2FS_FEATURE_ENCRYPT        0x0001
#define F2FS_FEATURE_BLKZONED     0x0002
#define F2FS_FEATURE_ATOMIC_WRITE 0x0004
#define F2FS_FEATURE_EXTRA_ATTR   0x0008
#define F2FS_FEATURE_PRJQUOTA     0x0010
#define F2FS_FEATURE_INODE_CHKSUM 0x0020
#define F2FS_FEATURE_COMPRESSION  0x0040
#define F2FS_FEATURE_FLEXIBLE_INLINE_XATTR 0x0040
#define F2FS_FEATURE_QUOTA_INO    0x0080
#define F2FS_FEATURE_INODE_CRTIME 0x0100
#define F2FS_FEATURE_SB_CHKSUM    0x0800
#define F2FS_FEATURE_CASEFOLD     0x1000

#define COLD_BIT_SHIFT          0
#define FSYNC_BIT_SHIFT         1
#define DENT_BIT_SHIFT          2
#define OFFSET_BIT_SHIFT        3

#define F2FS_INLINE_XATTR       0x01
#define F2FS_INLINE_DATA        0x02
#define F2FS_INLINE_DENTRY      0x04
#define F2FS_DATA_EXIST         0x08
#define F2FS_EXTRA_ATTR         0x20
#define F2FS_COMPR_FL           0x00000004

#define F2FS_SLOT_LEN           8
#define NR_DENTRY_IN_BLOCK      214
#define SIZE_OF_DIR_ENTRY       11
#define SIZE_OF_DENTRY_BITMAP   ((NR_DENTRY_IN_BLOCK + 7) / 8)
#define MAX_DIR_HASH_DEPTH      63
#define BITS_PER_BYTE           8

enum FILE_TYPE {
    F2FS_FT_UNKNOWN = 0,
    F2FS_FT_REG_FILE = 1,
    F2FS_FT_DIR = 2,
    F2FS_FT_CHRDEV = 3,
    F2FS_FT_BLKDEV = 4,
    F2FS_FT_FIFO = 5,
    F2FS_FT_SOCK = 6,
    F2FS_FT_SYMLINK = 7,
    F2FS_FT_MAX = 8
};

enum {
    CURSEG_HOT_DATA = 0,
    CURSEG_WARM_DATA = 1,
    CURSEG_COLD_DATA = 2,
    CURSEG_HOT_NODE = 3,
    CURSEG_WARM_NODE = 4,
    CURSEG_COLD_NODE = 5,
    NR_CURSEG_TYPE = 6
};

struct f2fs_device {
    u8 path[MAX_PATH_LEN];
    u32 total_segments;
} __attribute__((packed));

struct f2fs_super_block {
    u32 magic;
    u16 major_ver;
    u16 minor_ver;
    u32 log_sectorsize;
    u32 log_sectors_per_block;
    u32 log_blocksize;
    u32 log_blocks_per_seg;
    u32 segs_per_sec;
    u32 secs_per_zone;
    u32 checksum_offset;
    u64 block_count;
    u32 section_count;
    u32 segment_count;
    u32 segment_count_ckpt;
    u32 segment_count_sit;
    u32 segment_count_nat;
    u32 segment_count_ssa;
    u32 segment_count_main;
    u32 segment0_blkaddr;
    u32 cp_blkaddr;
    u32 sit_blkaddr;
    u32 nat_blkaddr;
    u32 ssa_blkaddr;
    u32 main_blkaddr;
    u32 root_ino;
    u32 node_ino;
    u32 meta_ino;
    u8 uuid[16];
    u16 volume_name[MAX_VOLUME_NAME];
    u32 extension_count;
    u8 extension_list[F2FS_MAX_EXTENSION][F2FS_EXTENSION_LEN];
    u32 cp_payload;
    u8 version[VERSION_LEN];
    u8 init_version[VERSION_LEN];
    u32 feature;
    u8 encryption_level;
    u8 encrypt_pw_salt[16];
    struct f2fs_device devs[MAX_DEVICES];
    u32 qf_ino[3];
    u8 hot_ext_count;
    u16 s_encoding;
    u16 s_encoding_flags;
    u8 s_stop_reason[32];
    u8 s_errors[16];
    u8 reserved[258];
    u32 crc;
} __attribute__((packed));

struct f2fs_checkpoint {
    u64 checkpoint_ver;
    u64 user_block_count;
    u64 valid_block_count;
    u32 rsvd_segment_count;
    u32 overprov_segment_count;
    u32 free_segment_count;
    u32 cur_node_segno[8];
    u16 cur_node_blkoff[8];
    u32 cur_data_segno[8];
    u16 cur_data_blkoff[8];
    u32 ckpt_flags;
    u32 cp_pack_total_block_count;
    u32 cp_pack_start_sum;
    u32 valid_node_count;
    u32 valid_inode_count;
    u32 next_free_nid;
    u32 sit_ver_bitmap_bytesize;
    u32 nat_ver_bitmap_bytesize;
    u32 checksum_offset;
    u64 elapsed_time;
    unsigned char alloc_type[16];
    unsigned char sit_nat_version_bitmap[];
} __attribute__((packed));

struct f2fs_nat_entry {
    u8 version;
    u32 ino;
    u32 block_addr;
} __attribute__((packed));

#define NAT_ENTRY_PER_BLOCK (F2FS_BLKSIZE / sizeof(struct f2fs_nat_entry))
struct f2fs_nat_block {
    struct f2fs_nat_entry entries[NAT_ENTRY_PER_BLOCK];
} __attribute__((packed));

#define SIT_VBLOCK_MAP_SIZE 64
struct f2fs_sit_entry {
    u16 vblocks;
    u8 valid_map[SIT_VBLOCK_MAP_SIZE];
    u64 mtime;
} __attribute__((packed));

#define SIT_ENTRY_PER_BLOCK (F2FS_BLKSIZE / sizeof(struct f2fs_sit_entry))
struct f2fs_sit_block {
    struct f2fs_sit_entry entries[SIT_ENTRY_PER_BLOCK];
} __attribute__((packed));

struct f2fs_summary {
    u32 nid;
    union {
        u8 reserved[3];
        struct {
            u8 version;
            u16 ofs_in_node;
        } __attribute__((packed));
    };
} __attribute__((packed));

struct summary_footer {
    unsigned char entry_type;
    u32 check_sum __attribute__((packed));
};

#define ENTRIES_IN_SUM (F2FS_BLKSIZE / 8)
struct f2fs_summary_block {
    struct f2fs_summary entries[ENTRIES_IN_SUM];
} __attribute__((packed));

struct f2fs_extent {
    u32 fofs;
    u32 blk_addr;
    u32 len;
} __attribute__((packed));

struct node_footer {
    u32 nid;
    u32 ino;
    u32 flag;
    u64 cp_ver __attribute__((packed));
    u32 next_blkaddr;
} __attribute__((packed));

#define OFFSET_OF_END_OF_I_EXT    360
#define SIZE_OF_I_NID             20
#define DEF_ADDRS_PER_INODE ((F2FS_BLKSIZE - OFFSET_OF_END_OF_I_EXT - SIZE_OF_I_NID - sizeof(struct node_footer)) / sizeof(u32))
#define DEF_ADDRS_PER_BLOCK ((F2FS_BLKSIZE - sizeof(struct node_footer)) / sizeof(u32))
#define NIDS_PER_BLOCK ((F2FS_BLKSIZE - sizeof(struct node_footer)) / sizeof(u32))
#define DEFAULT_INLINE_XATTR_ADDRS 50

struct f2fs_inode {
    u16 i_mode;
    u8 i_advise;
    u8 i_inline;
    u32 i_uid;
    u32 i_gid;
    u32 i_links;
    u64 i_size;
    u64 i_blocks;
    u64 i_atime;
    u64 i_ctime;
    u64 i_mtime;
    u32 i_atime_nsec;
    u32 i_ctime_nsec;
    u32 i_mtime_nsec;
    u32 i_generation;
    union {
        u32 i_current_depth;
        u16 i_gc_failures;
    };
    u32 i_xattr_nid;
    u32 i_flags;
    u32 i_pino;
    u32 i_namelen;
    u8 i_name[F2FS_NAME_LEN];
    u8 i_dir_level;
    struct f2fs_extent i_ext __attribute__((packed));
    union {
        struct {
            u16 i_extra_isize;
            u16 i_inline_xattr_size;
            u32 i_projid;
            u32 i_inode_checksum;
            u64 i_crtime;
            u32 i_crtime_nsec;
            u64 i_compr_blocks;
            u8 i_compress_algorithm;
            u8 i_log_cluster_size;
            u16 i_compress_flag;
            u32 i_extra_end[0];
        } __attribute__((packed));
        u32 i_addr[DEF_ADDRS_PER_INODE];
    };
} __attribute__((packed));

struct direct_node {
    u32 addr[DEF_ADDRS_PER_BLOCK];
} __attribute__((packed));

struct indirect_node {
    u32 nid[NIDS_PER_BLOCK];
} __attribute__((packed));

struct f2fs_node {
    union {
        struct f2fs_inode i;
        struct direct_node dn;
        struct indirect_node in;
    };
} __attribute__((packed));

#define F2FS_NODE_FOOTER(blk) ((struct node_footer *) \
    (((char *)(&(blk)->i)) + F2FS_BLKSIZE - sizeof(struct node_footer)))

struct f2fs_dir_entry {
    u32 hash_code;
    u32 ino;
    u16 name_len;
    u8 file_type;
} __attribute__((packed));

struct f2fs_dentry_block {
    u8 dentry_bitmap[SIZE_OF_DENTRY_BITMAP];
    u8 reserved[3];
    struct f2fs_dir_entry dentry[NR_DENTRY_IN_BLOCK];
    u8 filename[NR_DENTRY_IN_BLOCK][F2FS_SLOT_LEN];
} __attribute__((packed));

#define F2FS_DENTRY_BLOCK_FILENAME(blk, i) (&((u8 *)&(blk)->dentry[NR_DENTRY_IN_BLOCK])[(i) * F2FS_SLOT_LEN])

#define F2FS_COMPRESS_LZO           0
#define F2FS_COMPRESS_LZ4           1
#define F2FS_COMPRESS_ZSTD          2
#define F2FS_COMPRESS_LZORLE        3

_Static_assert(sizeof(struct f2fs_super_block) == 3072,
               "f2fs_super_block must be 3072 bytes");
_Static_assert(sizeof(struct f2fs_nat_entry) == 9,
               "f2fs_nat_entry must be 9 bytes");
_Static_assert(sizeof(struct f2fs_sit_entry) == 74,
               "f2fs_sit_entry must be 74 bytes (u16 + 64 bytes + u64)");
_Static_assert(sizeof(struct f2fs_extent) == 12,
               "f2fs_extent must be 12 bytes");
_Static_assert(sizeof(struct node_footer) == 24,
               "node_footer must be 24 bytes");
_Static_assert(sizeof(struct f2fs_summary) == 7,
               "f2fs_summary is 7 bytes when packed");
