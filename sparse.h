#pragma once

#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>

static const uint32_t SPARSE_MAGIC = 0xed26ff3a;
static const uint32_t SAMSUNG_SPARSE_MAGIC = 0xF7776F58;
static const uint32_t SPARSE_MAJOR_VER = 1;
static const uint16_t CHUNK_TYPE_RAW = 0xCAC1;
static const uint16_t CHUNK_TYPE_FILL = 0xCAC2;
static const uint16_t CHUNK_TYPE_DONT_CARE = 0xCAC3;
static const uint16_t CHUNK_TYPE_CRC32 = 0xCAC4;

typedef struct {
    uint32_t magic;
    uint16_t major_version;
    uint16_t minor_version;
    uint16_t file_hdr_sz;
    uint16_t chunk_hdr_sz;
    uint32_t blk_sz;
    uint32_t total_blks;
    uint32_t total_chunks;
    uint32_t image_checksum;
} SPARSE_t;

typedef struct {
    uint16_t chunk_type;
    uint16_t reserved1;
    uint32_t chunk_sz;
    uint32_t total_sz;
} chunk_header_t;

typedef struct {
    uint32_t blk_start;
    uint32_t blk_count;
    uint16_t type;
    uint64_t file_offset;
    uint32_t fill_val;
} sparse_chunk_t;

typedef struct sparse_ctx sparse_ctx_t;

struct sparse_ctx {
    FILE *fp;
    uint32_t blk_sz;
    uint32_t total_blks;
    uint32_t num_chunks;
    sparse_chunk_t *chunks;
    uint32_t cached_blk;
    uint8_t cached_data[4096];
};

bool sparse_detect(const char *path);

sparse_ctx_t *sparse_open(const char *path);

void sparse_close(sparse_ctx_t *ctx);

ssize_t sparse_read(sparse_ctx_t *ctx, uint64_t offset, void *buf, size_t size);

int sparse_read_block(sparse_ctx_t *ctx, uint32_t blkaddr, void *buf);

static inline uint64_t sparse_size(const sparse_ctx_t *ctx) {
    return (uint64_t)ctx->total_blks * ctx->blk_sz;
}

_Static_assert(sizeof(SPARSE_t) == 28, "SPARSE_t must be 28 bytes");
_Static_assert(sizeof(chunk_header_t) == 12, "chunk_header_t must be 12 bytes");
_Static_assert(sizeof(sparse_chunk_t) == 32, "sparse_chunk_t is 32 bytes");
