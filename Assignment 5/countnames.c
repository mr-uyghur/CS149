/*
 * Author names: Frederick Nguyen | Ali Halmamat
 * Author emails: quangnguyen1705@gmail.com | ali.Halmamat@sjsu.edu
 * Last modified date: 04/16/2026
 * Creation date: 04/13/2026
 *
 * countnames.c
 * Child process: reads one input file, counts name occurrences,
 * and stores results in the mmap'd shared memory segment provided
 * by the parent via file descriptor (passed as argv[1]).
 *
 * Assignment 5: Uses mmap for child results (same as A4),
 * but parent aggregates into malloc'd hash table.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>

/* Maximum entries a single child can store in its mmap segment */
#define MAX_NAMES  200
#define NAME_LEN   100

/* Shared memory record layout (same as A4) */
typedef struct {
    char name[NAME_LEN];
    int  count;
} NameEntry;

int main(int argc, char *argv[])
{
    if (argc != 3) {
        fprintf(stderr, "Usage: countnames <input_file> <shm_fd>\n");
        return 1;
    }

    const char *filename = argv[1];
    int shm_fd = atoi(argv[2]);

    /* Map the shared memory segment handed to us by the parent */
    size_t shm_size = sizeof(NameEntry) * MAX_NAMES;
    NameEntry *table = (NameEntry *)mmap(NULL, shm_size,
                                         PROT_READ | PROT_WRITE,
                                         MAP_SHARED, shm_fd, 0);
    if (table == MAP_FAILED) {
        perror("countnames: mmap");
        return 1;
    }
    close(shm_fd); /* fd no longer needed after mmap */

    /* Zero-initialise the segment */
    memset(table, 0, shm_size);

    /* Open the input file */
    FILE *fp = fopen(filename, "r");
    if (!fp) {
        perror("countnames: fopen");
        munmap(table, shm_size);
        return 1;
    }

    char  *line   = NULL;   /* getline manages this buffer */
    size_t buflen = 0;
    int    nslots = 0;      /* number of distinct names stored so far */

    while (getline(&line, &buflen, fp) != -1) {
        /* Strip trailing newline / carriage-return */
        size_t len = strlen(line);
        while (len > 0 && (line[len-1] == '\n' || line[len-1] == '\r'))
            line[--len] = '\0';

        if (len == 0)
            continue; /* skip blank lines */

        /* Search for existing entry */
        int found = 0;
        for (int i = 0; i < nslots; i++) {
            if (strcmp(table[i].name, line) == 0) {
                table[i].count++;
                found = 1;
                break;
            }
        }

        if (!found) {
            if (nslots >= MAX_NAMES) {
                fprintf(stderr, "countnames: too many distinct names in %s\n",
                        filename);
                break;
            }
            /* Copy name safely */
            strncpy(table[nslots].name, line, NAME_LEN - 1);
            table[nslots].name[NAME_LEN - 1] = '\0';
            table[nslots].count = 1;
            nslots++;
        }
    }

    free(line);      /* free getline's buffer */
    fclose(fp);
    munmap(table, shm_size);

    return 0;
}