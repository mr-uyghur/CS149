/**
 * Description:
 * --------------
 * countnames reads names from a file (or stdin) and counts
 * how many times each name appears.
 *
 * For Assignment 2:
 * - This program processes ONE file at a time.
 * - Output is written to stdout.
 * - Warnings and errors are written to stderr.
 * - The parent shell process redirects stdout/stderr
 *   to PID.out and PID.err.
 *
 * Exit Codes:
 * - 0 : success
 * - 1 : file open error or internal error
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define MAX_NAMES 100        /* maximum distinct names allowed */
#define MAX_NAME_LEN 30      /* maximum length of a single name */
#define MAX_LEN (MAX_NAME_LEN + 1)
#define BUF_SIZE 256         /* buffer size for reading lines */

/* Structure used to store a name and its count */
typedef struct {
    char name[MAX_LEN];
    int count;
} NameCount;

/*
 * strip_newline
 * --------------
 * Removes trailing '\n' and '\r' characters
 * from a string after using fgets().
 */
static void strip_newline(char *s) {
    size_t n = strlen(s);

    while (n > 0 && (s[n - 1] == '\n' || s[n - 1] == '\r')) {
        s[n - 1] = '\0';
        n--;
    }
}

int main(int argc, char *argv[]) {

    FILE *fp = NULL;        /* file pointer for input */
    char *filename = NULL;  /* used for warning messages */

    /*
     * Input Handling:
     * ---------------
     * If a filename is given → open file.
     * If no filename → read from stdin.
     * If too many arguments → exit with error.
     */
    if (argc == 2) {

        filename = argv[1];

        fp = fopen(filename, "r");
        if (fp == NULL) {
            /* Exact required error message format */
            fprintf(stderr, "error: cannot open file %s\n", filename);
            return 1;
        }
    }
    else if (argc == 1) {
        /* No filename → read from standard input */
        fp = stdin;
    }
    else {
        /* Invalid usage */
        return 1;
    }

    /* Array to store distinct names and their counts */
    NameCount names[MAX_NAMES];
    int distinct = 0;  /* number of distinct names found */

    char buffer[BUF_SIZE];
    int line_num = 0;  /* track line numbers for warnings */

    /*
     * Read file line-by-line using fgets()
     */
    while (fgets(buffer, sizeof(buffer), fp) != NULL) {

        line_num++;

        /* Remove newline characters */
        strip_newline(buffer);

        /*
         * If line is empty, print warning and continue.
         * Empty lines are ignored for counting purposes.
         */
        if (strlen(buffer) == 0) {
            if (filename != NULL) {
                fprintf(stderr,
                        "Warning - file %s line %d is empty.\n",
                        filename,
                        line_num);
            }
            continue;
        }

        /*
         * Truncate name if it exceeds MAX_NAME_LEN.
         * Ensures consistent storage and comparison.
         */
        if (strlen(buffer) > MAX_NAME_LEN) {
            buffer[MAX_NAME_LEN] = '\0';
        }

        /*
         * Search for existing name in our array.
         */
        int found_index = -1;

        for (int i = 0; i < distinct; i++) {
            if (strcmp(names[i].name, buffer) == 0) {
                found_index = i;
                break;
            }
        }

        /*
         * If name exists → increment count.
         * Otherwise → add new name to array.
         */
        if (found_index >= 0) {

            names[found_index].count++;

        } else {

            /* Check capacity limit */
            if (distinct >= MAX_NAMES) {
                fprintf(stderr,
                        "Error: Too many distinct names (max %d).\n",
                        MAX_NAMES);

                if (fp != stdin)
                    fclose(fp);

                return 1;
            }

            /* Store new name */
            strncpy(names[distinct].name, buffer, MAX_NAME_LEN);
            names[distinct].name[MAX_NAME_LEN] = '\0';
            names[distinct].count = 1;
            distinct++;
        }
    }

    /* Close file if it is not stdin */
    if (fp != stdin)
        fclose(fp);

    /*
     * Print results to stdout.
     * Order is based on first appearance in input.
     */
    for (int i = 0; i < distinct; i++) {
        printf("%s: %d\n", names[i].name, names[i].count);
    }

    return 0;
}