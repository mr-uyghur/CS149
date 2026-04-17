/*
 * shell.c  –  Assignment 5
 *
 * Author names: Frederick Nguyen | Ali Halmamat
 * Author emails: quangnguyen1705@gmail.com | ali.Halmamat@sjsu.edu
 * Last modified date: 04/13/2026
 * Creation date: 04/13/2026
 *
 * Parent (shell) process:
 *   1. Allocates one mmap'd shared-memory segment per input file.
 *   2. Forks one child per file; each child exec's ./countnames which writes
 *      its per-file name counts into the segment.
 *   3. All children run IN PARALLEL — the parent spawns every child before
 *      calling wait(), then reaps them with wait() (not waitpid) so it collects
 *      whichever child finishes first.
 *   4. After all children exit the parent aggregates results into a dynamically
 *      malloc'd hash table (malloc / realloc / strdup).
 *   5. Prints aggregated counts, then frees every byte of allocated memory.
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <fcntl.h>

/* ------------------------------------------------------------------ */
/*  Constants                                                          */
/* ------------------------------------------------------------------ */

/* Max distinct names one child can store in its mmap segment */
#define MAX_NAMES_PER_CHILD  200
#define NAME_LEN             100

/* Initial bucket count for the parent's dynamic hash table */
#define INITIAL_BUCKETS      10

/* ------------------------------------------------------------------ */
/*  Shared-memory record layout  (must match countnames.c)            */
/* ------------------------------------------------------------------ */

typedef struct {
    char name[NAME_LEN];
    int  count;
} NameEntry;

/* ------------------------------------------------------------------ */
/*  Parent's dynamic hash-table node                                  */
/* ------------------------------------------------------------------ */

typedef struct NameCountData {
    char                 *name;  /* heap string – allocated by strdup()  */
    int                   count;
    struct NameCountData *next;  /* singly-linked chain per bucket        */
} NameCountData;

/* ------------------------------------------------------------------ */
/*  Cleanup helper – called on every error path                       */
/* ------------------------------------------------------------------ */

/*
 * cleanup() unmaps and closes every segment that was successfully
 * created (indices 0 .. nsegments-1), then frees the two helper arrays.
 * Call before any early return so valgrind sees zero leaks.
 */
static void cleanup(int *shm_fds, NameEntry **shm_maps,
                    int nsegments, size_t shm_size)
{
    for (int i = 0; i < nsegments; i++) {
        if (shm_maps[i] != NULL)
            munmap(shm_maps[i], shm_size);
        if (shm_fds[i] != -1)
            close(shm_fds[i]);
    }
    free(shm_fds);
    free(shm_maps);
}

/* ------------------------------------------------------------------ */
/*  Hash-table: insert / resize / free / print                        */
/* ------------------------------------------------------------------ */

/*
 * djb2 hash — maps a name string to a bucket index in [0, nbuckets).
 */
static unsigned int hash_name(const char *name, int nbuckets)
{
    unsigned long h = 5381;
    int c;
    while ((c = (unsigned char)*name++) != 0)
        h = ((h << 5) + h) + c;   /* h = h*33 + c */
    return (unsigned int)(h % (unsigned long)nbuckets);
}

/*
 * table_insert() — add `delta` to the count for `name`.
 *
 * If the name is new, a fresh NameCountData node is malloc'd and its
 * name field is filled via strdup() (heap copy, NOT a char array).
 *
 * When the number of distinct names exceeds the bucket count the bucket
 * array is doubled with realloc() and all nodes are rehashed.
 *
 * Returns the (possibly reallocated) table pointer.
 */
static NameCountData **table_insert(NameCountData **table,
                                    int            *nbuckets,
                                    const char     *name,
                                    int             delta)
{
    unsigned int idx = hash_name(name, *nbuckets);

    /* ---- Search the chain for an existing entry ---- */
    NameCountData *node = table[idx];
    while (node != NULL) {
        if (strcmp(node->name, name) == 0) {
            node->count += delta;
            return table;           /* found – no structural change */
        }
        node = node->next;
    }

    /* ---- Not found: allocate a new node ---- */
    NameCountData *newnode = (NameCountData *)malloc(sizeof(NameCountData));
    if (newnode == NULL) {
        perror("malloc: new hash node");
        exit(1);
    }
    /*
     * strdup() allocates a fresh heap buffer and copies the string.
     * We must NOT pre-malloc the pointer; strdup does it internally.
     * We must free(node->name) later — see table_free().
     */
    newnode->name = strdup(name);
    if (newnode->name == NULL) {
        perror("strdup");
        free(newnode);
        exit(1);
    }
    newnode->count = delta;
    newnode->next  = table[idx];   /* prepend to bucket chain */
    table[idx]     = newnode;

    /* ---- Count total distinct names to decide whether to resize ---- */
    int total = 0;
    for (int b = 0; b < *nbuckets; b++) {
        NameCountData *n = table[b];
        while (n != NULL) { total++; n = n->next; }
    }

    /* ---- Resize: double bucket array when load factor exceeds 1 ---- */
    if (total > *nbuckets) {
        int old_n = *nbuckets;
        int new_n = old_n * 2;

        /*
         * realloc() may return a different pointer — always save to a
         * new variable; never use the old pointer after this call.
         */
        NameCountData **newtable = (NameCountData **)realloc(
            table, sizeof(NameCountData *) * new_n);
        if (newtable == NULL) {
            perror("realloc: bucket array");
            exit(1);
        }
        /* Zero-initialise the newly added slots */
        for (int b = old_n; b < new_n; b++)
            newtable[b] = NULL;

        *nbuckets = new_n;
        table     = newtable;

        /*
         * Rehash: collect every existing node, clear all buckets,
         * then re-insert each node at its new bucket position.
         */
        NameCountData **all = (NameCountData **)malloc(
            sizeof(NameCountData *) * total);
        if (all == NULL) { perror("malloc: rehash buffer"); exit(1); }

        int ai = 0;
        for (int b = 0; b < new_n; b++) {
            NameCountData *cur = table[b];
            while (cur != NULL) {
                all[ai++] = cur;
                cur = cur->next;
            }
            table[b] = NULL;        /* clear bucket before reinsertion */
        }
        for (int i = 0; i < ai; i++) {
            all[i]->next = NULL;
            unsigned int ni = hash_name(all[i]->name, new_n);
            all[i]->next = table[ni];
            table[ni]    = all[i];
        }
        free(all);                  /* free the temporary pointer array */
    }

    return table;
}

/*
 * table_free() — release every strdup'd name, every node, and the
 * bucket array itself.  After this call the pointer is invalid.
 */
static void table_free(NameCountData **table, int nbuckets)
{
    for (int b = 0; b < nbuckets; b++) {
        NameCountData *node = table[b];
        while (node != NULL) {
            NameCountData *next = node->next;
            free(node->name);   /* free the strdup'd string */
            free(node);         /* free the node struct     */
            node = next;
        }
    }
    free(table);                /* free the bucket pointer array */
}

/*
 * table_print() — print every entry as "name: count\n".
 */
static void table_print(NameCountData **table, int nbuckets)
{
    for (int b = 0; b < nbuckets; b++) {
        NameCountData *node = table[b];
        while (node != NULL) {
            printf("%s: %d\n", node->name, node->count);
            node = node->next;
        }
    }
}

/* ------------------------------------------------------------------ */
/*  main                                                               */
/* ------------------------------------------------------------------ */

int main(int argc, char *argv[])
{
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <file1> [file2] [file3]\n", argv[0]);
        return 1;
    }

    int    nfiles   = argc - 1;
    size_t shm_size = sizeof(NameEntry) * MAX_NAMES_PER_CHILD;

    /* ----------------------------------------------------------------
     * Allocate the helper arrays. Initialise every element to a safe
     * sentinel (-1 / NULL) so cleanup() can skip un-initialised slots.
     * ---------------------------------------------------------------- */
    int        *shm_fds  = (int *)        malloc(sizeof(int)         * nfiles);
    NameEntry **shm_maps = (NameEntry **) malloc(sizeof(NameEntry *) * nfiles);

    if (shm_fds == NULL || shm_maps == NULL) {
        perror("malloc: helper arrays");
        free(shm_fds);    /* free(NULL) is defined – safe even if NULL */
        free(shm_maps);
        return 1;
    }

    /* Initialise to safe sentinels before any real allocation */
    for (int i = 0; i < nfiles; i++) {
        shm_fds[i]  = -1;
        shm_maps[i] = NULL;
    }

    /* ----------------------------------------------------------------
     * Create one anonymous shared-memory segment per input file.
     * memfd_create() produces a kernel-managed fd with no filesystem
     * entry.  The child process inherits the fd across fork() and
     * passes its number to countnames via execl().
     * ---------------------------------------------------------------- */
    for (int i = 0; i < nfiles; i++) {
        shm_fds[i] = memfd_create("shm_child", 0);
        if (shm_fds[i] == -1) {
            perror("memfd_create");
            cleanup(shm_fds, shm_maps, nfiles, shm_size);
            return 1;
        }

        if (ftruncate(shm_fds[i], (off_t)shm_size) == -1) {
            perror("ftruncate");
            cleanup(shm_fds, shm_maps, nfiles, shm_size);
            return 1;
        }

        /* Map the segment in the parent so we can read after children exit */
        shm_maps[i] = (NameEntry *)mmap(NULL, shm_size,
                                        PROT_READ | PROT_WRITE,
                                        MAP_SHARED, shm_fds[i], 0);
        if (shm_maps[i] == MAP_FAILED) {
            perror("mmap");
            shm_maps[i] = NULL;    /* reset to sentinel before cleanup */
            cleanup(shm_fds, shm_maps, nfiles, shm_size);
            return 1;
        }
    }

    /* ----------------------------------------------------------------
     * Spawn one child per file.
     * ALL forks happen BEFORE any wait() — this is what makes the
     * children run in parallel.
     *
     * If fork() fails mid-loop, we reap every child already launched
     * before returning (prevents zombie processes).
     * ---------------------------------------------------------------- */
    int children_spawned = 0;

    for (int i = 0; i < nfiles; i++) {
        pid_t pid = fork();

        if (pid < 0) {
            /* fork failed — reap every child already running */
            perror("fork");
            for (int k = 0; k < children_spawned; k++)
                wait(NULL);
            cleanup(shm_fds, shm_maps, nfiles, shm_size);
            return 1;
        }

        if (pid == 0) {
            /* ============================================================
             * CHILD process
             * ============================================================
             * Close every other child's fd/mapping — this child only
             * needs segment i.  Prevents the child from touching another
             * child's memory after exec.
             */
            for (int j = 0; j < nfiles; j++) {
                if (j != i) {
                    munmap(shm_maps[j], shm_size);
                    close(shm_fds[j]);
                }
            }
            /*
             * Save the fd number as a string BEFORE freeing the array.
             * Unmap parent's view of OUR segment before exec;
             * countnames will create its own fresh mapping from the fd.
             */
            char fd_str[16];
            snprintf(fd_str, sizeof(fd_str), "%d", shm_fds[i]);
            munmap(shm_maps[i], shm_size);
            free(shm_fds);
            free(shm_maps);

            execl("./countnames", "./countnames", argv[i + 1], fd_str, NULL);
            perror("execl");    /* only reached if execl fails */
            exit(1);
        }

        /* Parent: child was forked successfully */
        children_spawned++;
    }

    /* ----------------------------------------------------------------
     * Wait for ALL children — using wait() not waitpid(fixed_pid).
     *
     * wait() reaps WHICHEVER child exits next (true parallel harvest).
     * waitpid(pids[i]) would force sequential reaping in spawn order,
     * blocking unnecessarily if a later child finishes first.
     * ---------------------------------------------------------------- */
    for (int i = 0; i < children_spawned; i++) {
        int   status;
        pid_t finished = wait(&status);
        if (finished == -1) {
            perror("wait");
            break;
        }
        if (WIFEXITED(status) && WEXITSTATUS(status) != 0)
            fprintf(stderr, "warning: child PID %d exited with status %d\n",
                    finished, WEXITSTATUS(status));
    }

    /* ----------------------------------------------------------------
     * Aggregate results from each child's mmap segment into the
     * parent's dynamically allocated hash table.
     *
     * The table starts at INITIAL_BUCKETS = 10 (malloc).
     * table_insert() doubles it with realloc() whenever needed.
     * ---------------------------------------------------------------- */
    int nbuckets = INITIAL_BUCKETS;
    NameCountData **table = (NameCountData **)malloc(
        sizeof(NameCountData *) * nbuckets);
    if (table == NULL) {
        perror("malloc: hash table");
        cleanup(shm_fds, shm_maps, nfiles, shm_size);
        return 1;
    }
    for (int b = 0; b < nbuckets; b++)
        table[b] = NULL;

    for (int i = 0; i < nfiles; i++) {
        for (int j = 0; j < MAX_NAMES_PER_CHILD; j++) {
            /* name[0] == '\0' is the sentinel marking end of data */
            if (shm_maps[i][j].name[0] == '\0')
                break;
            table = table_insert(table, &nbuckets,
                                 shm_maps[i][j].name,
                                 shm_maps[i][j].count);
        }
    }

    /* ----------------------------------------------------------------
     * Print, then free EVERYTHING in reverse allocation order.
     * valgrind --leak-check=full must report 0 bytes lost.
     * ---------------------------------------------------------------- */
    table_print(table, nbuckets);

    table_free(table, nbuckets);           /* hash nodes + strdup strings */
    cleanup(shm_fds, shm_maps, nfiles, shm_size); /* mmap + fds + arrays */

    return 0;
}