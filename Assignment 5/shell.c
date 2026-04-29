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

// REVIEW: We define _GNU_SOURCE at the top because we use memfd_create()
// later on, which is a Linux-specific function. Without this define,
// the compiler won't find that function declaration.
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

// REVIEW: These constants set the upper limits for how much data each
// child process can handle. MAX_NAMES_PER_CHILD caps distinct names at
// 200 per file, and NAME_LEN caps each name at 100 characters.
// These are intentionally generous for typical input but still bounded
// so we know exactly how much shared memory to allocate.
#define MAX_NAMES_PER_CHILD  200
#define NAME_LEN             100

// REVIEW: This is the starting size for our hash table in the parent.
// We start small at 10 buckets, but the table_insert function will
// double this automatically when the load factor exceeds 1. So we're
// not locked into this number — it's just a reasonable starting point.
#define INITIAL_BUCKETS      10

/* ------------------------------------------------------------------ */
/*  Shared-memory record layout  (must match countnames.c)            */
/* ------------------------------------------------------------------ */

// REVIEW: This struct is the contract between shell.c and countnames.c.
// Both files define the exact same layout — a fixed-size char array for
// the name and an int for the count. Because it's going into shared
// memory via mmap, using a fixed-size char array (not a pointer) is
// critical. A pointer would be meaningless across process boundaries
// because each process has its own virtual address space.
typedef struct {
    char name[NAME_LEN];
    int  count;
} NameEntry;

/* ------------------------------------------------------------------ */
/*  Parent's dynamic hash-table node                                  */
/* ------------------------------------------------------------------ */

// REVIEW: Now this struct is different from NameEntry above. This one
// is used ONLY in the parent process for the final aggregation step.
// Notice the key difference: the name field here is a char POINTER,
// not a fixed-size array. That's because in the parent's own address
// space, we can safely use heap-allocated strings via strdup(). We also
// have a 'next' pointer — this is a singly-linked list node, which is
// how we handle hash collisions using chaining.
typedef struct NameCountData {
    char                 *name;  /* heap string – allocated by strdup()  */
    int                   count;
    struct NameCountData *next;  /* singly-linked chain per bucket        */
} NameCountData;

/* ------------------------------------------------------------------ */
/*  Cleanup helper – called on every error path                       */
/* ------------------------------------------------------------------ */

// REVIEW: This is our centralized cleanup function. Anytime something
// goes wrong — whether it's a failed mmap, a failed fork, whatever —
// we call this to unmap shared memory segments, close file descriptors,
// and free the helper arrays. The key insight here is that we initialized
// everything to safe sentinels (-1 for fds, NULL for pointers) so this
// function can safely skip over slots that were never used. This pattern
// prevents resource leaks on every error path.
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

// REVIEW: This is djb2, a classic string hashing function created by
// Daniel J. Bernstein. The magic number 5381 is the initial hash value,
// and each character gets folded in with the formula h*33 + c. The bit
// shift (h << 5) plus h is a fast way to compute h*33. At the end we
// mod by the bucket count to get an index. It's simple, fast, and gives
// a good distribution for typical string data.
static unsigned int hash_name(const char *name, int nbuckets)
{
    unsigned long h = 5381;
    int c;
    while ((c = (unsigned char)*name++) != 0)
        h = ((h << 5) + h) + c;   /* h = h*33 + c */
    return (unsigned int)(h % (unsigned long)nbuckets);
}

// REVIEW: table_insert is the heart of our hash table implementation.
// It does three things: (1) searches for an existing name and bumps its
// count if found, (2) allocates a new node with malloc + strdup if the
// name is new, and (3) checks whether the hash table has gotten too full
// and resizes it by doubling the bucket array with realloc. Let's walk
// through each part.
static NameCountData **table_insert(NameCountData **table,
                                    int            *nbuckets,
                                    const char     *name,
                                    int             delta)
{
    unsigned int idx = hash_name(name, *nbuckets);

    // REVIEW: First we walk the linked list (the chain) at bucket[idx]
    // looking for a matching name. If we find it, we just add delta to
    // the existing count and return immediately — no allocation needed.
    NameCountData *node = table[idx];
    while (node != NULL) {
        if (strcmp(node->name, name) == 0) {
            node->count += delta;
            return table;           /* found – no structural change */
        }
        node = node->next;
    }

    // REVIEW: If we get here, the name wasn't in the table. So we
    // malloc a new NameCountData node. Notice we're checking for NULL
    // right after — good practice, because malloc CAN fail if you're
    // out of memory.
    NameCountData *newnode = (NameCountData *)malloc(sizeof(NameCountData));
    if (newnode == NULL) {
        perror("malloc: new hash node");
        exit(1);
    }
    // REVIEW: Here's where strdup comes in. strdup() is essentially
    // malloc + strcpy rolled into one — it allocates a new buffer on
    // the heap and copies the string into it. This is important because
    // the 'name' pointer we received might point to shared memory that
    // we're about to unmap. We need our own persistent copy. And since
    // strdup calls malloc internally, we have to remember to free this
    // later — which table_free() handles.
    newnode->name = strdup(name);
    if (newnode->name == NULL) {
        perror("strdup");
        free(newnode);
        exit(1);
    }
    newnode->count = delta;
    // REVIEW: We prepend the new node to the front of the bucket's chain.
    // This is O(1) insertion — we just set the new node's next pointer to
    // the current head of the list, then make the new node the head.
    newnode->next  = table[idx];
    table[idx]     = newnode;

    // REVIEW: After every insertion, we count the total number of distinct
    // names across ALL buckets. This is our load factor check. If total
    // exceeds the number of buckets, that means on average each bucket has
    // more than one entry, so collisions are getting frequent. Time to
    // resize.
    int total = 0;
    for (int b = 0; b < *nbuckets; b++) {
        NameCountData *n = table[b];
        while (n != NULL) { total++; n = n->next; }
    }

    // REVIEW: Here's the resize logic. We double the bucket count and
    // use realloc() to grow the array. This is one of the key requirements
    // of the assignment — demonstrating realloc. Notice we save the result
    // of realloc to a NEW variable 'newtable', not back into 'table'
    // directly. That's because if realloc fails it returns NULL, and if
    // we'd written that NULL over 'table', we'd lose our only pointer
    // to the existing data — classic memory leak bug.
    if (total > *nbuckets) {
        int old_n = *nbuckets;
        int new_n = old_n * 2;

        NameCountData **newtable = (NameCountData **)realloc(
            table, sizeof(NameCountData *) * new_n);
        if (newtable == NULL) {
            perror("realloc: bucket array");
            exit(1);
        }
        // REVIEW: The newly added bucket slots (from old_n to new_n - 1)
        // contain garbage values after realloc. We MUST zero them out,
        // otherwise we'd be dereferencing garbage pointers when we try
        // to traverse those chains later.
        for (int b = old_n; b < new_n; b++)
            newtable[b] = NULL;

        *nbuckets = new_n;
        table     = newtable;

        // REVIEW: After resizing, we need to rehash everything. The
        // bucket index for each name depends on the bucket count (because
        // of the modulo in hash_name), so when we double the buckets,
        // names might belong in different buckets now. We collect all
        // existing nodes into a temporary array, clear every bucket,
        // then reinsert each node at its new computed position.
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

// REVIEW: table_free walks every bucket, and for each node in the chain,
// it frees BOTH the strdup'd name string AND the node struct itself.
// The order matters here — we save node->next BEFORE freeing the node,
// otherwise we'd be reading freed memory. Finally we free the bucket
// array itself. After this call, the table pointer is completely invalid.
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

// REVIEW: Simple traversal — just iterate every bucket, walk each chain,
// and print "name: count". Nothing fancy here.
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
    // REVIEW: Standard argument check — we need at least one filename.
    // If the user runs "./shell1" with no arguments, we print usage and
    // exit. argv[0] is the program name, so the actual filenames start
    // at argv[1].
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <file1> [file2] [file3]\n", argv[0]);
        return 1;
    }

    // REVIEW: nfiles is the number of input files, and shm_size is how
    // many bytes each shared-memory segment needs. Each segment holds
    // MAX_NAMES_PER_CHILD entries of type NameEntry, so this gives us
    // the exact byte count for mmap.
    int    nfiles   = argc - 1;
    size_t shm_size = sizeof(NameEntry) * MAX_NAMES_PER_CHILD;

    // REVIEW: We dynamically allocate two parallel arrays — one for the
    // file descriptors and one for the mmap pointers. We use malloc here
    // (not stack arrays) because the assignment requires demonstrating
    // dynamic memory management. We also check that both mallocs succeed
    // before proceeding. Note the comment about free(NULL) being safe —
    // that's defined by the C standard, so if one malloc succeeded but
    // the other failed, we can safely free both without checking.
    int        *shm_fds  = (int *)        malloc(sizeof(int)         * nfiles);
    NameEntry **shm_maps = (NameEntry **) malloc(sizeof(NameEntry *) * nfiles);

    if (shm_fds == NULL || shm_maps == NULL) {
        perror("malloc: helper arrays");
        free(shm_fds);    /* free(NULL) is defined – safe even if NULL */
        free(shm_maps);
        return 1;
    }

    // REVIEW: This initialization loop is critical for correctness.
    // We set every fd to -1 and every map pointer to NULL. Why? Because
    // if something fails midway through the next loop, our cleanup()
    // function will iterate over ALL slots. If a slot was never actually
    // used, the sentinel values (-1 and NULL) tell cleanup to skip it.
    // Without this, cleanup might try to munmap a garbage pointer or
    // close a random fd.
    for (int i = 0; i < nfiles; i++) {
        shm_fds[i]  = -1;
        shm_maps[i] = NULL;
    }

    // REVIEW: Now we create one shared memory segment per input file.
    // We use memfd_create(), which is a Linux-specific call that creates
    // an anonymous file backed by memory — no actual file on disk.
    // The key advantage over shm_open is that we don't have to worry
    // about naming collisions in /dev/shm. The fd it returns is
    // inheritable by child processes across fork(), which is exactly
    // what we need.
    for (int i = 0; i < nfiles; i++) {
        shm_fds[i] = memfd_create("shm_child", 0);
        if (shm_fds[i] == -1) {
            perror("memfd_create");
            cleanup(shm_fds, shm_maps, nfiles, shm_size);
            return 1;
        }

        // REVIEW: memfd_create gives us a zero-length fd, so we have to
        // grow it with ftruncate to the size we actually need. Without
        // this, mmap would fail because there's no backing storage.
        if (ftruncate(shm_fds[i], (off_t)shm_size) == -1) {
            perror("ftruncate");
            cleanup(shm_fds, shm_maps, nfiles, shm_size);
            return 1;
        }

        // REVIEW: Now we mmap the segment in the parent. PROT_READ |
        // PROT_WRITE gives us read-write access, and MAP_SHARED is the
        // critical flag — it means changes made by child processes will
        // be visible to the parent. If we used MAP_PRIVATE instead,
        // each process would get its own copy-on-write version, and the
        // parent would never see the child's data.
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

    // REVIEW: Here's where the parallel execution happens. We fork one
    // child per file, and — this is important — we do ALL the forks
    // BEFORE any wait() calls. That's what makes the children run in
    // parallel. If we'd done fork-then-wait inside the same loop, each
    // child would have to finish before the next one starts, which would
    // be sequential. The children_spawned counter tracks how many we
    // successfully forked, so if a fork fails midway, we know exactly
    // how many children to reap.
    int children_spawned = 0;

    for (int i = 0; i < nfiles; i++) {
        pid_t pid = fork();

        if (pid < 0) {
            // REVIEW: If fork fails, we can't just bail out — we'd leave
            // zombie processes. So we reap every child we already spawned,
            // then clean up shared memory and exit.
            perror("fork");
            for (int k = 0; k < children_spawned; k++)
                wait(NULL);
            cleanup(shm_fds, shm_maps, nfiles, shm_size);
            return 1;
        }

        if (pid == 0) {
            // REVIEW: We're now in the child process. The first thing we
            // do is close every OTHER child's shared memory fd and unmap
            // their segments. Child i only needs segment i — giving it
            // access to segment j would be a security/correctness risk.
            // This is good practice: principle of least privilege.
            for (int j = 0; j < nfiles; j++) {
                if (j != i) {
                    munmap(shm_maps[j], shm_size);
                    close(shm_fds[j]);
                }
            }
            // REVIEW: We convert the fd number to a string so we can pass
            // it as a command-line argument to countnames via execl.
            // Important: we save the fd string BEFORE freeing the arrays
            // below. Also notice we unmap our OWN segment here too — the
            // child doesn't need the parent's mapping because countnames
            // will create its own fresh mmap from the fd.
            char fd_str[16];
            snprintf(fd_str, sizeof(fd_str), "%d", shm_fds[i]);
            munmap(shm_maps[i], shm_size);
            free(shm_fds);
            free(shm_maps);

            // REVIEW: execl replaces this child's entire process image
            // with the countnames program. We pass three arguments: the
            // program path, the input filename, and the fd string. The
            // NULL terminates the argument list. If execl succeeds, the
            // code below it NEVER runs. If it fails (e.g., countnames
            // binary is missing), we perror and exit.
            execl("./countnames", "./countnames", argv[i + 1], fd_str, NULL);
            perror("execl");    /* only reached if execl fails */
            exit(1);
        }

        /* Parent: child was forked successfully */
        children_spawned++;
    }

    // REVIEW: Now the parent waits for all children. We use wait() — not
    // waitpid with a specific PID. The difference is important: wait()
    // returns WHICHEVER child finishes next. If we had stored PIDs in an
    // array and called waitpid(pids[0]), then waitpid(pids[1]), etc., we'd
    // block on pids[0] even if pids[1] finished first. Using wait() gives
    // us true parallel harvesting — the order we reap depends on which
    // child actually finishes first, not the order we spawned them.
    for (int i = 0; i < children_spawned; i++) {
        int   status;
        pid_t finished = wait(&status);
        if (finished == -1) {
            perror("wait");
            break;
        }
        // REVIEW: We check the exit status of each child. WIFEXITED tells
        // us the child terminated normally (not by a signal), and
        // WEXITSTATUS extracts the exit code. If a child returned non-zero,
        // we print a warning but keep going — one failed file shouldn't
        // necessarily crash the whole aggregation.
        if (WIFEXITED(status) && WEXITSTATUS(status) != 0)
            fprintf(stderr, "warning: child PID %d exited with status %d\n",
                    finished, WEXITSTATUS(status));
    }

    // REVIEW: All children are done. Their results are sitting in the
    // mmap'd segments. Now we aggregate everything into a single hash
    // table. We start with INITIAL_BUCKETS (10) slots, all set to NULL.
    // As we insert names, table_insert will automatically double the
    // table via realloc when the load factor exceeds 1.
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

    // REVIEW: We iterate over every child's shared memory segment and
    // insert each name-count pair into our hash table. The sentinel is
    // name[0] == '\0' — when countnames runs out of names to store, the
    // remaining entries in the segment are still zero'd out from memset,
    // so we just break when we hit the first empty slot.
    for (int i = 0; i < nfiles; i++) {
        for (int j = 0; j < MAX_NAMES_PER_CHILD; j++) {
            if (shm_maps[i][j].name[0] == '\0')
                break;
            table = table_insert(table, &nbuckets,
                                 shm_maps[i][j].name,
                                 shm_maps[i][j].count);
        }
    }

    // REVIEW: Finally, we print the aggregated results, then free
    // EVERYTHING. The order here is important — we free the hash table
    // first (table_free handles nodes + strdup strings), then cleanup
    // handles the mmap segments, file descriptors, and helper arrays.
    // If you run this under valgrind with --leak-check=full, you should
    // see zero bytes lost, zero errors. That's how you know the memory
    // management is correct.
    table_print(table, nbuckets);

    table_free(table, nbuckets);           /* hash nodes + strdup strings */
    cleanup(shm_fds, shm_maps, nfiles, shm_size); /* mmap + fds + arrays */

    return 0;
}