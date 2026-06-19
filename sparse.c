#include "sparse.h"
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#ifdef _WIN32
#include <io.h>
#define fseeko _fseeki64
#define ftello _ftelli64
#endif

static inline uint32_t le32(uint32_t x) { return x; }
static inline uint16_t le16(uint16_t x) { return x; }

bool sparse_detect(const char *path) {
    FILE *fp = fopen(path, "rb");
    if (!fp) return false;
    uint32_t magic;
    bool is_sparse = false;
    if (fread(&magic, sizeof(magic), 1, fp) == 1) {
        uint32_t m = le32(magic);
        is_sparse = (m == SPARSE_MAGIC || m == SAMSUNG_SPARSE_MAGIC);
    }
    fclose(fp);
    return is_sparse;
}

sparse_ctx_t *sparse_open(const char *path) {
    FILE *fp = fopen(path, "rb");
    if (!fp) return NULL;
    
    SPARSE_t hdr;
    if (fseeko(fp, 0, SEEK_SET) != 0) goto fail;
    if (fread(&hdr, sizeof(hdr), 1, fp) != 1) goto fail;
    
    uint32_t magic = le32(hdr.magic);
    if (magic != SPARSE_MAGIC && magic != SAMSUNG_SPARSE_MAGIC) goto fail;
    if (le16(hdr.major_version) != SPARSE_MAJOR_VER) goto fail;
    
    uint32_t blk_sz = le32(hdr.blk_sz);
    uint32_t total_blks = le32(hdr.total_blks);
    uint32_t total_chunks = le32(hdr.total_chunks);
    
    if (blk_sz == 0 || blk_sz > 65536) goto fail;
    if (total_blks == 0) goto fail;
    if (total_chunks > 1000000) goto fail;
    
    sparse_ctx_t *ctx = calloc(1, sizeof(*ctx));
    if (!ctx) goto fail;
    
    ctx->fp = fp;
    ctx->blk_sz = blk_sz;
    ctx->total_blks = total_blks;
    ctx->num_chunks = total_chunks;
    ctx->cached_blk = UINT32_MAX;
    
    ctx->chunks = calloc(total_chunks, sizeof(sparse_chunk_t));
    if (!ctx->chunks) goto fail_ctx;
    
    uint16_t file_hdr_sz = le16(hdr.file_hdr_sz);
    uint16_t chunk_hdr_sz = le16(hdr.chunk_hdr_sz);
    
    if (fseeko(fp, file_hdr_sz, SEEK_SET) != 0) goto fail_chunks;
    
    uint32_t cur_blk = 0;
    uint64_t cur_off = file_hdr_sz;
    
    for (uint32_t i = 0; i < total_chunks; i++) {
        chunk_header_t chk;
        if (fread(&chk, sizeof(chk), 1, fp) != 1) goto fail_chunks;
        
        uint16_t chunk_type = le16(chk.chunk_type);
        uint32_t chunk_sz = le32(chk.chunk_sz);
        uint32_t total_sz = le32(chk.total_sz);
        uint32_t data_sz = total_sz - chunk_hdr_sz;
        
        sparse_chunk_t *chunk = &ctx->chunks[i];
        chunk->blk_start = cur_blk;
        chunk->blk_count = chunk_sz;
        chunk->type = chunk_type;
        
        switch (chunk_type) {
            case CHUNK_TYPE_RAW:
                if (data_sz != chunk_sz * blk_sz) goto fail_chunks;
                chunk->file_offset = cur_off + chunk_hdr_sz;
                if (fseeko(fp, data_sz, SEEK_CUR) != 0) goto fail_chunks;
                break;
            case CHUNK_TYPE_FILL:
                if (data_sz != 4) goto fail_chunks;
                if (fread(&chunk->fill_val, 4, 1, fp) != 1) goto fail_chunks;
                chunk->fill_val = le32(chunk->fill_val);
                break;
            case CHUNK_TYPE_DONT_CARE:
                if (data_sz != 0) goto fail_chunks;
                chunk->fill_val = 0;
                break;
            case CHUNK_TYPE_CRC32:
                if (data_sz != 4) goto fail_chunks;
                if (fseeko(fp, 4, SEEK_CUR) != 0) goto fail_chunks;
                chunk->blk_count = 0;
                cur_blk -= chunk_sz;
                break;
            default:
                goto fail_chunks;
        }
        
        cur_blk += chunk_sz;
        cur_off += total_sz;
    }
    
    return ctx;

fail_chunks:
    free(ctx->chunks);
fail_ctx:
    free(ctx);
fail:
    fclose(fp);
    return NULL;
}

void sparse_close(sparse_ctx_t *ctx) {
    if (!ctx) return;
    if (ctx->fp) fclose(ctx->fp);
    if (ctx->chunks) free(ctx->chunks);
    free(ctx);
}

static sparse_chunk_t *find_chunk(sparse_ctx_t *ctx, uint32_t blk) {
    int left = 0, right = ctx->num_chunks - 1;
    while (left <= right) {
        int mid = left + (right - left) / 2;
        sparse_chunk_t *chunk = &ctx->chunks[mid];
        if (blk >= chunk->blk_start && blk < chunk->blk_start + chunk->blk_count)
            return chunk;
        else if (blk < chunk->blk_start)
            right = mid - 1;
        else
            left = mid + 1;
    }
    return NULL;
}

static int read_cached(sparse_ctx_t *ctx, uint32_t blk, void *buf) {
    if (ctx->cached_blk == blk) {
        memcpy(buf, ctx->cached_data, ctx->blk_sz);
        return 0;
    }
    
    sparse_chunk_t *chunk = find_chunk(ctx, blk);
    if (!chunk) {
        memset(buf, 0, ctx->blk_sz);
        ctx->cached_blk = blk;
        memcpy(ctx->cached_data, buf, ctx->blk_sz);
        return 0;
    }
    
    uint32_t off = blk - chunk->blk_start;
    
    switch (chunk->type) {
        case CHUNK_TYPE_RAW: {
            uint64_t file_off = chunk->file_offset + (uint64_t)off * ctx->blk_sz;
            if (fseeko(ctx->fp, file_off, SEEK_SET) != 0) return -EIO;
            if (fread(buf, ctx->blk_sz, 1, ctx->fp) != 1) return -EIO;
            break;
        }
        case CHUNK_TYPE_FILL: {
            uint32_t *ptr = (uint32_t *)buf;
            uint32_t words = ctx->blk_sz / 4;
            for (uint32_t i = 0; i < words; i++) {
                ptr[i] = chunk->fill_val;
            }
            break;
        }
        case CHUNK_TYPE_DONT_CARE:
            memset(buf, 0, ctx->blk_sz);
            break;
        case CHUNK_TYPE_CRC32:
            memset(buf, 0, ctx->blk_sz);
            break;
        default:
            return -EINVAL;
    }
    
    ctx->cached_blk = blk;
    memcpy(ctx->cached_data, buf, ctx->blk_sz);
    return 0;
}

int sparse_read_block(sparse_ctx_t *ctx, uint32_t blkaddr, void *buf) {
    if (!ctx || !buf) return -EINVAL;
    if (blkaddr >= ctx->total_blks) return -EINVAL;
    return read_cached(ctx, blkaddr, buf);
}

ssize_t sparse_read(sparse_ctx_t *ctx, uint64_t offset, void *buf, size_t size) {
    if (!ctx || !buf) return -EINVAL;
    uint64_t total = (uint64_t)ctx->total_blks * ctx->blk_sz;
    if (offset >= total) return 0;
    if (offset + size > total) size = total - offset;

    uint8_t *out = (uint8_t *)buf;
    size_t total_read = 0;
    
    while (total_read < size) {
        uint64_t cur_off = offset + total_read;
        uint32_t blk = cur_off / ctx->blk_sz;
        uint32_t blk_off = cur_off % ctx->blk_sz;
        size_t to_read = ctx->blk_sz - blk_off;
        if (to_read > size - total_read) to_read = size - total_read;

        if (blk_off != 0 || to_read < ctx->blk_sz) {
            uint8_t tmp[4096];
            if (read_cached(ctx, blk, tmp) != 0) return -EIO;
            memcpy(out + total_read, tmp + blk_off, to_read);
        } else {
            if (read_cached(ctx, blk, out + total_read) != 0) return -EIO;
        }
        total_read += to_read;
    }
    return total_read;
}
