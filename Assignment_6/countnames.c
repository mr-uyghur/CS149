/*
 * countnames.c - Assignment 6
 *
 * Author names: Frederick Nguyen | Ali Halmamat
 * Author emails: quangnguyen1705@gmail.com | ali.Halmamat@sjsu.edu
 * Last modified date: 04/17/2026
 * Creation date: 04/17/2026
 *
 * Multi-threaded name counting using pthreads and mutex locking.
 *
 * Each thread reads one input file and updates a shared name-count
 * array.  A mutex protects the shared array so updates are atomic.
 *
 * Compile:
 *   gcc -D_REENTRANT -pthread -o countnames countnames.c -Wall -Werror
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <pthread.h>

/* ---------- data structures ---------- */

#define NAME_LEN 128

typedef struct {
    char  name[NAME_LEN];
    int   count;
} NameCount;

/* Shared array of name-count pairs (malloc'd by main) */
static NameCount *namecounts = NULL;
static int        num_names  = 0;    /* current number of distinct names */
static int        capacity   = 0;    /* allocated slots                  */

/* Mutex that protects namecounts / num_names / capacity */
static pthread_mutex_t nc_mutex = PTHREAD_MUTEX_INITIALIZER;

/* ---------- per-thread argument ---------- */

typedef struct {
    const char *filename;
} ThreadArg;

/*
 * trim_newline - strip trailing '\n' or '\r' from s in-place.
 */
static void trim_newline(char *s)
{
    size_t len = strlen(s);
    while (len > 0 && (s[len - 1] == '\n' || s[len - 1] == '\r')) {
        s[--len] = '\0';
    }
}

/*
 * update_count - look up `name` in the shared array and increment its
 * counter, or append a new entry.  MUST be called with nc_mutex held.
 */
static void update_count(const char *name)
{
    /* search for existing entry */
    for (int i = 0; i < num_names; i++) {
        if (strcmp(namecounts[i].name, name) == 0) {
            namecounts[i].count++;
            return;
        }
    }

    /* new name – grow array if needed */
    if (num_names == capacity) {
        capacity = (capacity == 0) ? 16 : capacity * 2;
        NameCount *tmp = realloc(namecounts, capacity * sizeof(NameCount));
        if (tmp == NULL) {
            perror("realloc");
            /* unlock before exit — if we called exit() while still holding
             * nc_mutex, every other thread blocked on lock() would hang
             * forever (deadlock).  Releasing first lets them observe the
             * process termination cleanly. */
            pthread_mutex_unlock(&nc_mutex);
            exit(EXIT_FAILURE);
        }
        namecounts = tmp;
    }

    /* add the new entry */
    strncpy(namecounts[num_names].name, name, NAME_LEN - 1);
    namecounts[num_names].name[NAME_LEN - 1] = '\0';
    namecounts[num_names].count = 1;
    num_names++;
}

/* ---------- thread function ---------- */

/*
 * thread_func - reads names from the file given in arg and updates the
 * shared namecounts array using mutex-protected critical sections.
 */
static void *thread_func(void *arg)
{
    ThreadArg *targ = (ThreadArg *)arg;
    const char *filename = targ->filename;

    FILE *fp = fopen(filename, "r");
    if (fp == NULL) {
        /* non-fatal: print error and exit this thread */
        fprintf(stderr, "Error: cannot open file '%s'\n", filename);
        return NULL;
    }

    char   *line = NULL;
    size_t  linecap = 0;
    ssize_t nread;

    /* read one name per line until EOF */
    while ((nread = getline(&line, &linecap, fp)) != -1) {
        trim_newline(line);

        /* skip empty lines */
        if (line[0] == '\0') {
            continue;
        }

        /* --- critical section: update shared array --- */
        pthread_mutex_lock(&nc_mutex);
        update_count(line);
        pthread_mutex_unlock(&nc_mutex);
        /* --- end critical section --- */
    }

    free(line);   /* free the buffer allocated by getline (once) */
    fclose(fp);
    return NULL;
}

/* ---------- main ---------- */

int main(int argc, char *argv[])
{
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <file1> [file2 ...]\n", argv[0]);
        return EXIT_FAILURE;
    }

    int num_files = argc - 1;

    /* allocate thread handles, created-flags, and argument structs */
    pthread_t *threads = malloc(num_files * sizeof(pthread_t));
    bool      *created = malloc(num_files * sizeof(bool));
    ThreadArg *args    = malloc(num_files * sizeof(ThreadArg));
    if (threads == NULL || created == NULL || args == NULL) {
        perror("malloc");
        free(threads);
        free(created);
        free(args);
        return EXIT_FAILURE;
    }

    /* create one thread per input file */
    for (int i = 0; i < num_files; i++) {
        args[i].filename = argv[i + 1];
        int rc = pthread_create(&threads[i], NULL, thread_func, &args[i]);
        if (rc != 0) {
            fprintf(stderr, "pthread_create failed for '%s': %s\n",
                    argv[i + 1], strerror(rc));
            created[i] = false;  /* mark as not started so we skip join */
        } else {
            created[i] = true;
        }
    }

    /* wait for all successfully-created threads to finish */
    for (int i = 0; i < num_files; i++) {
        if (created[i]) {
            pthread_join(threads[i], NULL);
        }
    }

    /* print aggregated results */
    for (int i = 0; i < num_names; i++) {
        printf("%s: %d\n", namecounts[i].name, namecounts[i].count);
    }

    /* clean up */
    free(namecounts);
    free(threads);
    free(created);
    free(args);
    pthread_mutex_destroy(&nc_mutex);

    return EXIT_SUCCESS;
}