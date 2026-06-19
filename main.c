#include "extract.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#define VERSION "1.0.1"

void usage(const char *prog) {
    fprintf(stderr,
        "Usage: %s [option] <f2fs_image> <output_dir>\n"
        "Options:\n"
        "  -h    print help\n"
        "  -v    print image info\n"
        "  -V    print version\n",prog);
}

int main(int argc, char *argv[]) {
    bool verbose = false;
    const char *img = NULL;
    const char *outdir = NULL;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0) {
            usage(argv[0]);
            return 0;
        } else if (strcmp(argv[i], "-v") == 0) {
            verbose = true;
        } else if (strcmp(argv[i], "-V") == 0) {
            fprintf(stderr, VERSION);
            return 0;
        } else if (!img) {
            img = argv[i];
        } else if (!outdir) {
            outdir = argv[i];
        } else {
            fprintf(stderr, "Error: Too many args\n");
            usage(argv[0]);
            return 1;
        }
    }
    if (!img || !outdir) {
        fprintf(stderr, "Error: Miss required args\n");
        usage(argv[0]);
        return 1;
    }
    f2fs_info_t *info = f2fs_init(img);
    if (!info) {
        fprintf(stderr, "Error: Fail to open f2fs image\n");
        return 1;
    }
    if (verbose) {
        print_info(info);
    }
    nid_t root = le32_to_cpu(info->sb->root_ino);
    int ret = extract_tree(info, root, outdir);
    f2fs_cleanup(info);
    return (ret == 0) ? 0 : 1;
}
