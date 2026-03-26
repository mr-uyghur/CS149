
/**
 * Description:
 * --------------
 * countnames reads names from a file (or stdin) and counts
 * how many times each name appears.
 *
 * Author names: Frederick Nguyen | Ali Halmamat
 * Author emails: quangnguyen1705@gmail.com | ali.Halmamat@sjsu.edu
 * Last modified date: 03/21/2026
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

static void strip_newline(char *s) {
    size_t len = strlen(s);
    while (len > 0 && (s[len - 1] == '\n' || s[len - 1] == '\r')) {
        s[len - 1] = '\0';
        len--;
    }
}

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

    for (int i = 0; i < MAX_NAMES; i++) {
        table[i].name[0] = '\0';
        table[i].count = 0;
    }

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
        src_label = argv[1];
        in = fopen(argv[1], "r");
        if (in == NULL) {
            fprintf(errf, "error: cannot open file %s\n", argv[1]);
            fclose(outf);
            fclose(errf);
            return 1;
        }
    } else if (argc == 1) {
        in = stdin;
    } else {
        fclose(outf);
        fclose(errf);
        return 0;
    }

    while (fgets(buf, sizeof(buf), in) != NULL) {
        line_num++;
        strip_newline(buf);

        if (strlen(buf) < 1) {
            fprintf(errf, "Warning - file %s line %d is empty.\n", src_label, line_num);
            continue;
        }

        if (strlen(buf) > MAX_NAME_LEN) {
            buf[MAX_NAME_LEN] = '\0';
        }

        int found = 0;
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

            strncpy(table[distinct].name, buf, sizeof(table[distinct].name) - 1);
            table[distinct].name[sizeof(table[distinct].name) - 1] = '\0';
            table[distinct].count = 1;
            distinct++;
        }
    }

    if (in != stdin) {
        fclose(in);
    }

    /* write per-child result to PID.out */
    for (int i = 0; i < distinct; i++) {
        fprintf(outf, "%s: %d\n", table[i].name, table[i].count);
    }

    fclose(outf);
    fclose(errf);

    /*
     * stdout of child has been redirected by shell.c into a pipe.
     * So sending to STDOUT_FILENO sends data back to the parent.
     */
    for (int i = 0; i < distinct; i++) {
        if (write_struct_namecount(STDOUT_FILENO, &table[i]) < 0) {
            return 1;
        }
    }

    return 0;
}