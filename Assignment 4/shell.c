/*
 * shell.c - Assignment 4
 * Author: Frederick Nguyen and Ali Halmamat
 * Parent shell process that:
 *   1. Creates a large GLOBAL shared memory region using mmap (anonymous, MAP_SHARED).
 *   2. Divides GLOBAL into one sub-region per child process (each MAX_NAMES entries).
 *   3. Spawns one child countnames process per input file using fork() + execvp().
 *   4. Passes each child: its filename, the shm fd, its byte offset, and region size.
 *   5. Waits for ALL children to finish (wait() loop).
 *   6. Aggregates each child's results into a summation region in GLOBAL.
 *   7. Prints the final combined name counts to stdout.
 *
 * Usage:
 *   ./countnames names1.txt names2.txt names3.txt ...
 *
 * Compile:
 *   gcc -o shell shell.c -Wall -Werror
 *
 * Notes:
 *   - We use MAP_ANONYMOUS | MAP_SHARED so the mapping is inherited across fork().
 *   - After execvp() the child reconnects using /proc/self/fd/<fd> (Linux) because
 *     anonymous mmap regions are not preserved across exec. The fd IS preserved
 *     (unless O_CLOEXEC is set). We pass the fd number via argv so the child can
 *     call mmap() again on the same fd.
 *   - For anonymous mmap, the fd must be -1 normally, but to share across exec we
 *     use memfd_create() which gives a real (inheritable) fd backed by anonymous memory.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <fcntl.h>
#include <sys/syscall.h>

#define MAX_NAMES    100
#define MAX_LEN      31   /* 30 chars + null terminator */
#define MAX_FILES    64   /* maximum number of input files */

#ifdef __linux__
static inline int my_memfd_create(const char *name, unsigned int flags) {
    return syscall(SYS_memfd_create, name, flags);
}
#endif

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
     * Create shared memory using memfd_create (Linux) so the fd survives
     * execvp() in child processes.
     * ------------------------------------------------------------------ */
#ifdef __linux__
    int shm_fd = my_memfd_create("countnames_shm", 0);
    if (shm_fd == -1) {
        perror("memfd_create");
        return 1;
    }
    /* Set the size of the memory object */
    if (ftruncate(shm_fd, (off_t)total_size) == -1) {
        perror("ftruncate");
        return 1;
    }
#else
    /* Fallback for non-Linux: use a temp file */
    char shm_path[] = "/tmp/countnames_shm_XXXXXX";
    int shm_fd = mkstemp(shm_path);
    if (shm_fd == -1) {
        perror("mkstemp");
        return 1;
    }
    unlink(shm_path); /* delete path; fd keeps it alive */
    if (ftruncate(shm_fd, (off_t)total_size) == -1) {
        perror("ftruncate");
        return 1;
    }
#endif

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
    int   child_ids[MAX_FILES]; /* maps pid -> child index (0-based) */

    /* Convert fd, offset, and region_size to strings for execvp argv */
    char fd_str[32];
    char region_str[32];
    snprintf(fd_str,     sizeof(fd_str),     "%d",  shm_fd);
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
             *   countnames <filename> <fd> <offset> <region_size>
             */
            char *child_argv[6];
            child_argv[0] = "./countnames";
            child_argv[1] = argv[i + 1];     /* input filename */
            child_argv[2] = fd_str;           /* shm fd number  */
            child_argv[3] = offset_str;       /* byte offset    */
            child_argv[4] = region_str;       /* region size    */
            child_argv[5] = NULL;

            execvp("./countnames", child_argv);
            /* If execvp returns, it failed */
            perror("execvp");
            exit(1);
        }

        /* ---- Parent: record the child's pid and index ---- */
        pids[i]      = pid;
        child_ids[i] = i;
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
                fname = argv[child_ids[i] + 1];
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
     * Clean up: unmap and close the shared memory fd.
     * ------------------------------------------------------------------ */
    munmap(GLOBAL, total_size);
    close(shm_fd);

    return 0;
}