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

// REVIEW: Standard includes — stdio for file I/O, stdlib for atoi and
// malloc, string.h for strcmp and strncpy, and then the system headers
// for mmap, file control, and POSIX I/O. Notice we DON'T need
// _GNU_SOURCE here because countnames doesn't use memfd_create — that's
// only in the parent (shell.c).
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>

// REVIEW: Constants must match shell.c exactly to ensure correct
// mmap layout alignment and prevent data corruption in IPC.
#define MAX_NAMES  200
#define NAME_LEN   100

// REVIEW: Same struct as in shell.c. Again, this is the shared-memory
// contract. A fixed-size char array (not a char pointer) is essential
// here because this struct lives in mmap'd memory that's shared between
// two completely separate processes. A pointer would only be valid in
// the address space of the process that created it.
typedef struct {
    char name[NAME_LEN];
    int  count;
} NameEntry;

int main(int argc, char *argv[])
{
    // REVIEW: This program expects exactly 2 arguments: the input
    // filename and the shared-memory file descriptor number. The parent
    // (shell.c) passes both of these via execl. If the argument count
    // is wrong, something went wrong in the parent, so we print usage
    // and bail.
    if (argc != 3) {
        fprintf(stderr, "Usage: countnames <input_file> <shm_fd>\n");
        return 1;
    }

    const char *filename = argv[1];
    // REVIEW: We use atoi to convert the fd string back to an integer.
    // The parent created this fd with memfd_create, and because fork()
    // preserves open file descriptors, the child inherited it. We just
    // need the number to use it.
    int shm_fd = atoi(argv[2]);

    // REVIEW: Here we create our OWN mmap of the shared memory segment.
    // The parent already unmapped its view before calling execl, so this
    // is a fresh mapping. MAP_SHARED is again the key flag — any writes
    // we make here will be visible to the parent when it reads the same
    // segment after we exit.
    size_t shm_size = sizeof(NameEntry) * MAX_NAMES;
    NameEntry *table = (NameEntry *)mmap(NULL, shm_size,
                                         PROT_READ | PROT_WRITE,
                                         MAP_SHARED, shm_fd, 0);
    if (table == MAP_FAILED) {
        perror("countnames: mmap");
        return 1;
    }
    // REVIEW: Once we've mmap'd the fd, we don't need the fd itself
    // anymore — the mapping persists independently. Closing it early is
    // good practice because it frees up a file descriptor slot.
    close(shm_fd);

    // REVIEW: We zero out the entire segment with memset. This is
    // important because it ensures that name[0] == '\0' for all unused
    // slots, which is the sentinel the parent uses to know where valid
    // data ends and empty slots begin.
    memset(table, 0, shm_size);

    // REVIEW: Standard file open. If the file doesn't exist or isn't
    // readable, we print an error, clean up the mmap, and exit with
    // status 1. The parent will see that non-zero exit status in the
    // wait loop and print a warning.
    FILE *fp = fopen(filename, "r");
    if (!fp) {
        perror("countnames: fopen");
        munmap(table, shm_size);
        return 1;
    }

    // REVIEW: We use getline() for reading lines. getline is nice because
    // it dynamically allocates and resizes the buffer as needed — we don't
    // have to guess a maximum line length. The 'line' pointer starts as
    // NULL and getline will malloc it for us. We just have to remember to
    // free it when we're done (which we do at the bottom).
    char  *line   = NULL;   /* getline manages this buffer */
    size_t buflen = 0;
    int    nslots = 0;      /* number of distinct names stored so far */

    while (getline(&line, &buflen, fp) != -1) {
        // REVIEW: getline includes the trailing newline in the buffer,
        // so we strip it here. We also strip carriage returns for
        // cross-platform compatibility (Windows line endings are \r\n).
        // We walk backwards from the end of the string, replacing
        // newline/CR characters with null terminators.
        size_t len = strlen(line);
        while (len > 0 && (line[len-1] == '\n' || line[len-1] == '\r'))
            line[--len] = '\0';

        // REVIEW: Skip blank lines — after stripping, if the line is
        // empty, there's nothing to count.
        if (len == 0)
            continue;

        // REVIEW: Linear search through the existing entries to see if
        // we've already seen this name. If found, we just increment its
        // count. This is O(n) per lookup, which is fine for up to 200
        // names — we're not trying to be fancy here since the real hash
        // table logic is in the parent.
        int found = 0;
        for (int i = 0; i < nslots; i++) {
            if (strcmp(table[i].name, line) == 0) {
                table[i].count++;
                found = 1;
                break;
            }
        }

        // REVIEW: If the name is new, we add it to the next available
        // slot. First we check that we haven't exceeded MAX_NAMES —
        // that's our hard limit for the shared memory segment. Then we
        // use strncpy with NAME_LEN - 1 to prevent buffer overflow, and
        // manually null-terminate. This is safer than strcpy, which
        // could write past the end of the fixed-size array if the input
        // name is really long.
        if (!found) {
            if (nslots >= MAX_NAMES) {
                fprintf(stderr, "countnames: too many distinct names in %s\n",
                        filename);
                break;
            }
            strncpy(table[nslots].name, line, NAME_LEN - 1);
            table[nslots].name[NAME_LEN - 1] = '\0';
            table[nslots].count = 1;
            nslots++;
        }
    }

    // REVIEW: Cleanup — we free the getline buffer, close the file, and
    // unmap the shared memory. Even though this process is about to exit
    // (which would clean up everything anyway), explicitly freeing is
    // good practice and means valgrind won't report any leaks if you
    // run it on this program.
    free(line);      /* free getline's buffer */
    fclose(fp);
    munmap(table, shm_size);

    return 0;
}