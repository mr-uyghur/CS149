
/**
 * Description:
 * --------------
 * countnames reads names from a file (or stdin) and counts
 * how many times each name appears.
 *
 * Author names: Frederick Nguyen | Ali Halmamat
 * Author emails: quangnguyen1705@gmail.com | ali.Halmamat@sjsu.edu
 * Last modified date: 03/25/2026
 * Creation date: 03/21/2026
 * For Assignment 3:
 * - This program processes ONE file at a time.
 * - Output is written to stdout.
 * - Warnings and errors are written to stderr.
 * - The parent shell process redirects stdout/stderr
 *   to PID.out and PID.err.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/*
 * Overview:
 * 1. Open one input source, either a file or stdin.
 * 2. Read names line by line and count repeated names in a local table.
 * 3. Write a readable summary to PID.out and warnings/errors to PID.err.
 * 4. Send each name/count pair back to the parent through stdout, which
 *    the parent shell has redirected into a pipe.
 */

#define MAX_NAMES 100
#define MAX_NAME_LEN 30
#define BUF_SIZE 256

typedef struct {
    char name[30];
    int count;
} NameCountData;

typedef enum {
    TYPE_NAMECOUNT,
    TYPE_B
} MessageType;

typedef struct {
    MessageType type;
    size_t size;
} MessageHeader;

/* Remove trailing newline characters so names compare consistently. */
static void strip_newline(char *s) {
    size_t len = strlen(s);
    while (len > 0 && (s[len - 1] == '\n' || s[len - 1] == '\r')) {
        s[len - 1] = '\0';
        len--;
    }
}

/* Write until the full buffer has been sent through the pipe. */
static ssize_t write_all(int fd, const void *buf, size_t count) {
    size_t total = 0;
    const char *p = (const char *)buf;

    while (total < count) {
        ssize_t n = write(fd, p + total, count - total);
        if (n <= 0) {
            return -1;
        }
        total += (size_t)n;
    }

    return (ssize_t)total;
}

/* Send one structured name/count record to the parent process. */
static int write_struct_namecount(int fd, const NameCountData *data) {
    MessageHeader header;
    header.type = TYPE_NAMECOUNT;
    header.size = sizeof(NameCountData);

    if (write_all(fd, &header, sizeof(header)) != (ssize_t)sizeof(header)) {
        return -1;
    }

    if (write_all(fd, data, sizeof(NameCountData)) != (ssize_t)sizeof(NameCountData)) {
        return -1;
    }

    return 0;
}

int main(int argc, char *argv[]) {
    FILE *in = NULL;
    FILE *outf = NULL;
    FILE *errf = NULL;
    char outname[64];
    char errname[64];
    pid_t pid = getpid();

    NameCountData table[MAX_NAMES];
    int distinct = 0;
    char buf[BUF_SIZE];
    int line_num = 0;
    const char *src_label = "stdin";

    /* Start with an empty table of distinct names for this child. */
    for (int i = 0; i < MAX_NAMES; i++) {
        table[i].name[0] = '\0';
        table[i].count = 0;
    }

    /* Build the child-specific output/error filenames using its PID. */
    snprintf(outname, sizeof(outname), "%d.out", (int)pid);
    snprintf(errname, sizeof(errname), "%d.err", (int)pid);

    outf = fopen(outname, "w");
    if (outf == NULL) {
        return 1;
    }

    errf = fopen(errname, "w");
    if (errf == NULL) {
        fclose(outf);
        return 1;
    }

    if (argc == 2) {
        /* File mode: process exactly one input file. */
        src_label = argv[1];
        in = fopen(argv[1], "r");
        if (in == NULL) {
            fprintf(errf, "error: cannot open file %s\n", argv[1]);
            fclose(outf);
            fclose(errf);
            return 1;
        }
    } else if (argc == 1) {
        /* stdin mode: read names directly from standard input. */
        in = stdin;
    } else {
        /* This program is designed to handle one file per execution. */
        fclose(outf);
        fclose(errf);
        return 0;
    }

    /* Read each line, normalize it, and update the local count table. */
    while (fgets(buf, sizeof(buf), in) != NULL) {
        line_num++;
        strip_newline(buf);

        /* Empty lines are ignored, but still reported as warnings. */
        if (strlen(buf) < 1) {
            fprintf(errf, "Warning - file %s line %d is empty.\n", src_label, line_num);
            continue;
        }

        /* Fit long names into the fixed-size assignment buffer. */
        if (strlen(buf) > MAX_NAME_LEN) {
            buf[MAX_NAME_LEN] = '\0';
        }

        int found = 0;
        /* Search the current table to see whether this name was seen before. */
        for (int i = 0; i < distinct; i++) {
            if (strcmp(table[i].name, buf) == 0) {
                table[i].count++;
                found = 1;
                break;
            }
        }

        if (!found) {
            if (distinct >= MAX_NAMES) {
                fprintf(errf, "error: too many distinct names\n");
                if (in != stdin) {
                    fclose(in);
                }
                fclose(outf);
                fclose(errf);
                return 1;
            }

            /* Add a brand new name entry when it does not exist yet. */
            strncpy(table[distinct].name, buf, sizeof(table[distinct].name) - 1);
            table[distinct].name[sizeof(table[distinct].name) - 1] = '\0';
            table[distinct].count = 1;
            distinct++;
        }
    }

    if (in != stdin) {
        fclose(in);
    }

    /* Save a readable per-file summary for this child process. */
    for (int i = 0; i < distinct; i++) {
        fprintf(outf, "%s: %d\n", table[i].name, table[i].count);
    }

    fclose(outf);
    fclose(errf);

    /*
     * The parent shell redirects the child's stdout into a pipe.
     * Writing to STDOUT_FILENO therefore sends structured results
     * back to the parent instead of printing them to the terminal.
     */
    for (int i = 0; i < distinct; i++) {
        if (write_struct_namecount(STDOUT_FILENO, &table[i]) < 0) {
            return 1;
        }
    }

    return 0;
}
