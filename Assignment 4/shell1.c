/*
 * shell1.c - Assignment 4
 * Author: Frederick Nguyen and Ali Halmamat
 * Parent shell1 process that:
 *   1. Creates a GLOBAL shared memory region using shm_open + mmap (MAP_SHARED).
 *   2. Divides GLOBAL into one sub-region per child process (each MAX_NAMES entries).
 *   3. Spawns one child countnames process per input file using fork() + execvp().
 *   4. Passes each child: its filename, the shm name, its byte offset, and region size.
 *   5. Waits for ALL children to finish (wait() loop).
 *   6. Aggregates each child's results into a summation region in GLOBAL.
 *   7. Prints the final combined name counts to stdout.
 *
 * Usage:
 *   ./countnames names1.txt names2.txt names3.txt ...
 *
 * Compile:
 *   gcc -o shell1 shell1.c -Wall -Werror -lrt
 *
 * Notes:
 *   - We use shm_open() to create a named shared memory object, then mmap() it.
 *   - After execvp(), children re-open the shared memory by calling shm_open()
 *     with the same name passed via argv. This is the standard POSIX approach
 *     for sharing memory across exec().
 *   - shm_unlink() is called at cleanup to remove the shared memory object.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#define MAX_NAMES    100
#define MAX_LEN      31   /* 30 chars + null terminator */
#define MAX_FILES    64   /* maximum number of input files */
#define SHM_NAME     "/countnames_shm"  /* name of the shared memory object */

/* Structure for one name/count entry (must match countnames.c) */
typedef struct {
    char name[MAX_LEN];
    int  count;
} NameCountData;

/*
 * aggregate_into_sum()
 * Reads up to MAX_NAMES entries from child_region (stopping at empty name),
 * and merges them into sum_region (also up to MAX_NAMES entries).
 * sum_distinct: current number of distinct names in sum_region (in/out).
 */
static void aggregate_into_sum(NameCountData *child_region,
                                NameCountData *sum_region,
                                int           *sum_distinct)
{
    for (int i = 0; i < MAX_NAMES; i++) {
        /* Sentinel: empty name means end of this child's list */
        if (child_region[i].name[0] == '\0') {
            break;
        }

        const char *cname = child_region[i].name;
        int         ccount = child_region[i].count;

        /* Search for existing name in sum_region */
        int found = 0;
        for (int j = 0; j < *sum_distinct; j++) {
            if (strcmp(sum_region[j].name, cname) == 0) {
                sum_region[j].count += ccount;
                found = 1;
                break;
            }
        }

        /* New name: add it */
        if (!found && *sum_distinct < MAX_NAMES) {
            strncpy(sum_region[*sum_distinct].name, cname, MAX_LEN - 1);
            sum_region[*sum_distinct].name[MAX_LEN - 1] = '\0';
            sum_region[*sum_distinct].count = ccount;
            (*sum_distinct)++;
        }
    }
}

int main(int argc, char *argv[])
{
    /* ------------------------------------------------------------------
     * If no filenames given, behave as standalone countnames (A1 mode):
     * read from stdin and exit.
     * ------------------------------------------------------------------ */
    if (argc == 1) {
        /* No files: just exit cleanly */
        return 0;
    }

    int num_files = argc - 1;  /* number of input files */

    /* ------------------------------------------------------------------
     * Calculate shared memory layout.
     *
     * GLOBAL layout:
     *   [ child_0_region | child_1_region | ... | child_(n-1)_region | sum_region ]
     *
     * Each region holds MAX_NAMES NameCountData entries.
     * ------------------------------------------------------------------ */
    size_t region_size  = (size_t)MAX_NAMES * sizeof(NameCountData);
    size_t total_size   = (size_t)(num_files + 1) * region_size; /* +1 for sum */

    /* ------------------------------------------------------------------
     * Create a named shared memory object using shm_open().
     * Children will re-open it by name after exec().
     * ------------------------------------------------------------------ */
    int shm_fd = shm_open(SHM_NAME, O_CREAT | O_RDWR, S_IRUSR | S_IWUSR);
    if (shm_fd == -1) {
        perror("shm_open");
        return 1;
    }
    /* Set the size of the shared memory object */
    if (ftruncate(shm_fd, (off_t)total_size) == -1) {
        perror("ftruncate");
        return 1;
    }

    /* ------------------------------------------------------------------
     * Map the entire GLOBAL region in the parent.
     * ------------------------------------------------------------------ */
    void *GLOBAL = mmap(NULL, total_size,
                        PROT_READ | PROT_WRITE,
                        MAP_SHARED,
                        shm_fd, 0);
    if (GLOBAL == MAP_FAILED) {
        perror("mmap GLOBAL");
        return 1;
    }

    /* Zero the entire region (ensures all sentinels start as '\0') */
    memset(GLOBAL, 0, total_size);

    /* ------------------------------------------------------------------
     * Spawn one child process per input file.
     * We fork ALL children first, then wait for any to finish (no bottleneck).
     * ------------------------------------------------------------------ */
    pid_t pid;                   /* reused by fork loop and wait loop */
    pid_t pids[MAX_FILES];

    /* Convert region_size to string for execvp argv */
    char region_str[32];
    snprintf(region_str, sizeof(region_str), "%zu", region_size);

    for (int i = 0; i < num_files; i++) {
        /* Byte offset for this child's region inside GLOBAL */
        size_t offset = (size_t)i * region_size;
        char offset_str[32];
        snprintf(offset_str, sizeof(offset_str), "%zu", offset);

        pid = fork();
        if (pid < 0) {
            perror("fork");
            return 1;
        }

        if (pid == 0) {
            /* ---- Child process ---- */
            /* Build argv for execvp:
             *   countnames <filename> <shm_name> <offset> <region_size>
             */
            char *child_argv[6];
            child_argv[0] = "./countnames";
            child_argv[1] = argv[i + 1];     /* input filename  */
            child_argv[2] = SHM_NAME;        /* shm object name */
            child_argv[3] = offset_str;       /* byte offset     */
            child_argv[4] = region_str;       /* region size     */
            child_argv[5] = NULL;

            execvp("./countnames", child_argv);
            /* If execvp returns, it failed */
            perror("execvp");
            exit(1);
        }

        /* ---- Parent: record the child's pid ---- */
        pids[i] = pid;
        printf("[shell] Spawned child PID %d for file: %s\n", pid, argv[i + 1]);
    }

    /* ------------------------------------------------------------------
     * Wait for ALL children to finish.
     * wait() returns the PID of whichever child finishes next, or <0
     * when there are no more children.  This loop reaps children in
     * completion order (fastest first) with no bottleneck.
     * ------------------------------------------------------------------ */
    int   status;
    while ((pid = wait(&status)) > 0) {

        /* Find which input file this pid was processing */
        const char *fname = "unknown";
        for (int i = 0; i < num_files; i++) {
            if (pids[i] == pid) {
                fname = argv[i + 1];
                break;
            }
        }

        if (WIFEXITED(status)) {
            fprintf(stderr, "Child %d terminated normally with exit code: %d\n",
                    pid, WEXITSTATUS(status));
        } else if (WIFSIGNALED(status)) {
            fprintf(stderr, "Child %d terminated abnormally with signal number: %d\n",
                    pid, WTERMSIG(status));
        }

        printf("[shell] Child PID %d (file: %s) done.\n", pid, fname);
    }

    /* ------------------------------------------------------------------
     * Aggregate results from each child's region into the summation region.
     * Summation region is at offset: num_files * region_size
     * ------------------------------------------------------------------ */
    NameCountData *sum_region =
        (NameCountData *)((char *)GLOBAL + (size_t)num_files * region_size);
    int sum_distinct = 0;

    for (int i = 0; i < num_files; i++) {
        NameCountData *child_region =
            (NameCountData *)((char *)GLOBAL + (size_t)i * region_size);
        aggregate_into_sum(child_region, sum_region, &sum_distinct);
    }

    /* ------------------------------------------------------------------
     * Print the final aggregated name counts to stdout.
     * ------------------------------------------------------------------ */
    printf("\n=== Final combined name counts ===\n");
    for (int i = 0; i < sum_distinct; i++) {
        printf("%s: %d\n", sum_region[i].name, sum_region[i].count);
    }

    /* ------------------------------------------------------------------
     * Clean up: unmap, close fd, and unlink the shared memory object.
     * shm_unlink removes the name; the region is freed once all
     * processes have unmapped it.
     * ------------------------------------------------------------------ */
    munmap(GLOBAL, total_size);
    close(shm_fd);
    shm_unlink(SHM_NAME);

    return 0;
}