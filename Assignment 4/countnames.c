/*
 * Author: Frederick Nguyen and Ali Halmamat
 * Created Date: 04/03/2026
 * Date Modified: 04/07/2026
 *
 *
 * countnames.c - Assignment 4
 * Counts occurrences of each name in an input file.
 * For A4, when called with a shared memory name and offset,
 * writes results directly into the assigned shared memory region.
 *
 *
 * Usage (A4-style, via shell1 with shared memory):
 *   ./countnames filename shm_name offset region_size
 *     filename    : input file to process
 *     shm_name    : name of the shared memory object (e.g. "/countnames_shm")
 *     offset      : byte offset into shared memory for this child's region
 *     region_size : size in bytes of this child's region
 *
 * Compile: gcc -o countnames countnames.c -Wall -Werror -lrt
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#define MAX_NAMES    100
#define MAX_LEN      31   /* 30 chars + null terminator */
#define BUFFER_SIZE  256

/* Structure to hold one name and its count */
typedef struct {
    char name[MAX_LEN];
    int  count;
} NameCountData;

/*
 * count_names_from_stream()
 * Reads names from 'fp', counts occurrences, stores in 'names[]'.
 * Returns number of distinct names found.
 * Prints warnings to stderr for empty lines.
 * 'filename' is used only for warning messages.
 */
static int count_names_from_stream(FILE *fp, NameCountData *names,
                                   int max_names, const char *filename)
{
    char   buffer[BUFFER_SIZE];
    int    distinct  = 0;
    int    line_num  = 0;

    while (fgets(buffer, sizeof(buffer), fp) != NULL) {
        line_num++;

        /* Strip trailing newline */
        size_t len = strlen(buffer);
        if (len > 0 && buffer[len - 1] == '\n') {
            buffer[--len] = '\0';
        }

        /* Skip (and warn about) empty lines */
        if (len < 1) {
            fprintf(stderr, "Warning - file %s line %d is empty.\n",
                    filename, line_num);
            continue;
        }

        /* Search for existing name */
        int found = 0;
        for (int i = 0; i < distinct; i++) {
            if (strcmp(names[i].name, buffer) == 0) {
                names[i].count++;
                found = 1;
                break;
            }
        }

        /* New distinct name */
        if (!found && distinct < max_names) {
            strncpy(names[distinct].name, buffer, MAX_LEN - 1);
            names[distinct].name[MAX_LEN - 1] = '\0';
            names[distinct].count = 1;
            distinct++;
        }
    }

    return distinct;
}

int main(int argc, char *argv[])
{
    /*
     * ---------- A4 shared-memory mode ----------
     * argv: countnames <filename> <shm_name> <offset> <region_size>
     *   argc == 5
     */
    if (argc == 5) {
        const char *filename    = argv[1];
        const char *shm_name    = argv[2];
        size_t      offset      = (size_t)atol(argv[3]);
        size_t      region_size = (size_t)atol(argv[4]);

        /* Re-open the shared memory object by name (created by parent shell1) */
        int shm_fd = shm_open(shm_name, O_RDWR, 0);
        if (shm_fd == -1) {
            perror("countnames: shm_open");
            return 1;
        }

        /* Open input file */
        FILE *fp = fopen(filename, "r");
        if (fp == NULL) {
            fprintf(stderr, "error: cannot open file %s\n", filename);
            return 1;
        }

        /*
         * Map from byte 0 of the fd up to offset + region_size.
         * We map the full span so the offset is always page-aligned at 0.
         * Then we advance the pointer by 'offset' bytes to reach our sub-region.
         */
        size_t map_size = offset + region_size;
        void *map = mmap(NULL, map_size,
                         PROT_READ | PROT_WRITE,
                         MAP_SHARED,
                         shm_fd, 0);
        if (map == MAP_FAILED) {
            perror("countnames: mmap");
            fclose(fp);
            close(shm_fd);
            return 1;
        }

        /* fd can be closed after mmap; the mapping persists independently */
        close(shm_fd);

        /* Advance to this child's assigned sub-region */
        NameCountData *region = (NameCountData *)((char *)map + offset);

        /* Zero the region before writing (marks end-of-list with name[0]=='\0') */
        memset(region, 0, region_size);

        /* Count names into a local array first */
        NameCountData local[MAX_NAMES];
        memset(local, 0, sizeof(local));
        int distinct = count_names_from_stream(fp, local, MAX_NAMES, filename);
        fclose(fp);

        /* Copy local results into shared memory region */
        int max_in_region = (int)(region_size / sizeof(NameCountData));
        int to_copy = (distinct < max_in_region - 1) ? distinct : max_in_region - 1;
        for (int i = 0; i < to_copy; i++) {
            region[i] = local[i];
        }
        /* Sentinel: entry after the last valid one has name[0] == '\0' */
        /* (already zeroed by memset, so sentinel is already in place)  */

        munmap(map, map_size);
        return 0;
    }

    /*
     * ---------- Standalone mode (A1 / A2 / A3 compatible) ----------
     * argc == 1 : read from stdin
     * argc == 2 : read from file given as argv[1]
     * argc > 2  : unknown, exit quietly
     */
    if (argc > 2) {
        return 0;
    }

    FILE       *fp       = NULL;
    const char *filename = "stdin";

    if (argc == 2) {
        filename = argv[1];
        fp = fopen(filename, "r");
        if (fp == NULL) {
            fprintf(stderr, "error: cannot open file\n");
            return 1;
        }
    } else {
        /* argc == 1: read from stdin */
        fp = stdin;
    }

    NameCountData names[MAX_NAMES];
    memset(names, 0, sizeof(names));

    int distinct = count_names_from_stream(fp, names, MAX_NAMES, filename);

    if (fp != stdin) {
        fclose(fp);
    }

    /* Print results to stdout */
    for (int i = 0; i < distinct; i++) {
        printf("%s: %d\n", names[i].name, names[i].count);
    }

    return 0;
}