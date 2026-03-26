/**
 * Description:
 * --------------
 * This program implements a minimal shell for Assignment 3.
 *
 * When the user types:
 *
 *   ./countnames file1.txt file2.txt file3.txt
 *
 * The shell will:
 *   - Fork once per input file
 *   - Each child executes ./countnames on ONE file
 *   - Redirect stdout to PID.out
 *   - Redirect stderr to PID.err
 *   - Parent waits for all children (parallel execution)
 *
 * This removes all APUE dependencies and uses standard POSIX calls.
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

/*
 * Overview:
 * 1. Repeatedly read a command from the user.
 * 2. If the command is countnames, create one child and one pipe per file.
 * 3. Let all children run in parallel, each processing one file.
 * 4. Read structured name/count messages from the pipes.
 * 5. Combine matching names from all children and print the final totals.
 */

#define MAX_LINE 1024
#define MAX_ARGS 256
#define MAX_NAMES 300

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

typedef struct {
    pid_t pid;
    int read_fd;
} ChildPipe;

/* Split the command line into argv-style tokens for execvp. */
static int split_line(char *line, char *argv[], int max_args) {
    int argc = 0;
    char *tok = strtok(line, " \t");
    while (tok != NULL && argc < max_args - 1) {
        argv[argc++] = tok;
        tok = strtok(NULL, " \t");
    }
    argv[argc] = NULL;
    return argc;
}

/* Treat either countnames or ./countnames as the A3 counting command. */
static int is_countnames_cmd(const char *cmd) {
    return strcmp(cmd, "./countnames") == 0 || strcmp(cmd, "countnames") == 0;
}

/* Add a child's result to the running total, or create a new total entry. */
static void add_or_sum(NameCountData totals[], int *distinct, const NameCountData *item) {
    for (int i = 0; i < *distinct; i++) {
        if (strcmp(totals[i].name, item->name) == 0) {
            totals[i].count += item->count;
            return;
        }
    }

    if (*distinct < MAX_NAMES) {
        strncpy(totals[*distinct].name, item->name, sizeof(totals[*distinct].name) - 1);
        totals[*distinct].name[sizeof(totals[*distinct].name) - 1] = '\0';
        totals[*distinct].count = item->count;
        (*distinct)++;
    }
}

/* Read until the requested number of bytes has been received. */
static ssize_t read_all(int fd, void *buf, size_t count) {
    size_t total = 0;
    char *p = (char *)buf;

    while (total < count) {
        ssize_t n = read(fd, p + total, count - total);
        if (n == 0) {
            return (ssize_t)total;   /* EOF */
        }
        if (n < 0) {
            return -1;
        }
        total += (size_t)n;
    }

    return (ssize_t)total;
}

/* Read every structured message from one child pipe and aggregate it. */
static void read_from_pipe(int fd, NameCountData totals[], int *distinct) {
    MessageHeader header;

    while (1) {
        ssize_t n = read_all(fd, &header, sizeof(header));
        if (n == 0) {
            return;   /* end of pipe */
        }
        if (n < 0 || n != (ssize_t)sizeof(header)) {
            fprintf(stderr, "Error reading message header from pipe\n");
            return;
        }

        switch (header.type) {
            case TYPE_NAMECOUNT: {
                NameCountData dataNC;

                if (header.size != sizeof(NameCountData)) {
                    fprintf(stderr, "Invalid payload size for TYPE_NAMECOUNT\n");
                    return;
                }

                n = read_all(fd, &dataNC, header.size);
                if (n < 0 || n != (ssize_t)header.size) {
                    fprintf(stderr, "Error reading NameCountData payload\n");
                    return;
                }

                add_or_sum(totals, distinct, &dataNC);
                break;
            }

            case TYPE_B: {
                /* future extension */
                char discard[256];
                size_t remaining = header.size;

                while (remaining > 0) {
                    size_t chunk = remaining < sizeof(discard) ? remaining : sizeof(discard);
                    ssize_t got = read_all(fd, discard, chunk);
                    if (got <= 0) {
                        fprintf(stderr, "Error reading TYPE_B payload\n");
                        return;
                    }
                    remaining -= (size_t)got;
                }
                break;
            }

            default:
                fprintf(stderr, "Unknown message type received: %d\n", header.type);
                return;
        }
    }
}

int main(void) {
    char line[MAX_LINE];

    while (1) {
        /* Show the prompt for the next shell command. */
        printf("%% ");
        fflush(stdout);

        if (fgets(line, sizeof(line), stdin) == NULL) {
            break;
        }

        line[strcspn(line, "\n")] = '\0';

        if (line[0] == '\0') {
            continue;
        }

        if (strcmp(line, "exit") == 0 || strcmp(line, "quit") == 0) {
            break;
        }

        char line_copy[MAX_LINE];
        char *args[MAX_ARGS];

        strncpy(line_copy, line, sizeof(line_copy) - 1);
        line_copy[sizeof(line_copy) - 1] = '\0';

        int argc = split_line(line_copy, args, MAX_ARGS);
        if (argc == 0) {
            continue;
        }

        /* For other commands, behave like a simple foreground shell. */
        if (!is_countnames_cmd(args[0])) {
            pid_t other = fork();
            if (other == 0) {
                execvp(args[0], args);
                _exit(1);
            } else if (other > 0) {
                waitpid(other, NULL, 0);
            }
            continue;
        }

        if (argc == 1) {
            pid_t pid = fork();
            if (pid == 0) {
                char *cn_argv[] = { (char *)"./countnames", NULL };
                execvp(cn_argv[0], cn_argv);
                _exit(1);
            } else if (pid > 0) {
                waitpid(pid, NULL, 0);
            }
            continue;
        }

        ChildPipe children[MAX_ARGS];
        int child_count = 0;
        NameCountData totals[MAX_NAMES];
        int total_distinct = 0;

        /* Clear the parent's aggregate table before this command runs. */
        for (int i = 0; i < MAX_NAMES; i++) {
            totals[i].name[0] = '\0';
            totals[i].count = 0;
        }

        /*
         * PARALLEL PART:
         * create all children first, do not wait here.
         */
        for (int i = 1; i < argc; i++) {
            int pipefd[2];

            if (pipe(pipefd) < 0) {
                perror("pipe");
                continue;
            }

            pid_t pid = fork();

            if (pid < 0) {
                perror("fork");
                close(pipefd[0]);
                close(pipefd[1]);
                continue;
            }

            if (pid == 0) {
                close(pipefd[0]);

                /*
                 * Replace stdout with write end of pipe.
                 * Child can now "print" / write to stdout and parent receives it.
                 */
                if (dup2(pipefd[1], STDOUT_FILENO) < 0) {
                    close(pipefd[1]);
                    _exit(1);
                }

                close(pipefd[1]);

                /* Each child executes countnames on exactly one input file. */
                char *cn_argv[] = { (char *)"./countnames", args[i], NULL };
                execvp(cn_argv[0], cn_argv);
                _exit(1);
            }

            /* Parent keeps only the read end so it can collect child results. */
            close(pipefd[1]);
            children[child_count].pid = pid;
            children[child_count].read_fd = pipefd[0];
            child_count++;
        }

        /*
         * Wait for children after all are spawned.
         * wait() returns PID of child that finished.
         */
        int status;
        pid_t pid;

        while ((pid = wait(&status)) > 0) {
            if (WIFEXITED(status)) {
                fprintf(stderr,
                        "Child %d terminated normally with exit code: %d\n",
                        pid, WEXITSTATUS(status));
            } else if (WIFSIGNALED(status)) {
                fprintf(stderr,
                        "Child %d terminated abnormally with signal number: %d\n",
                        pid, WTERMSIG(status));
            }

            /* Find the matching pipe for the finished child and read its data. */
            for (int i = 0; i < child_count; i++) {
                if (children[i].pid == pid) {
                    read_from_pipe(children[i].read_fd, totals, &total_distinct);
                    close(children[i].read_fd);
                    children[i].read_fd = -1;
                    break;
                }
            }
        }

        /* Print the final combined totals after every child has finished. */
        for (int i = 0; i < total_distinct; i++) {
            printf("%s: %d\n", totals[i].name, totals[i].count);
        }
    }

    return 0;
}
