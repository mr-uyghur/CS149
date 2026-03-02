/**
 * Description:
 * --------------
 * This program implements a minimal shell for Assignment 2.
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
#include <fcntl.h>

#define MAXLINE 1024   /* maximum input line length */
#define MAXARGS 100    /* maximum number of command arguments */

int main(void)
{
    char buf[MAXLINE];    /* buffer for user input */
    pid_t pid;
    int status;

    /* Print initial prompt */
    printf("%% ");
    fflush(stdout);

    /*
     * Main shell loop
     * Continues until EOF (Ctrl+D) or user types "exit"
     */
    while (fgets(buf, MAXLINE, stdin) != NULL) {

        /* Remove trailing newline character */
        if (buf[strlen(buf) - 1] == '\n')
            buf[strlen(buf) - 1] = '\0';

        /* If empty command, reprint prompt */
        if (strlen(buf) == 0) {
            printf("%% ");
            fflush(stdout);
            continue;
        }

        /* Exit command */
        if (strcmp(buf, "exit") == 0) {
            break;
        }

        /*
         * Tokenize input into arguments
         * Example:
         *   "./countnames file1.txt file2.txt"
         * becomes:
         *   args[0] = "./countnames"
         *   args[1] = "file1.txt"
         *   args[2] = "file2.txt"
         */
        char *args[MAXARGS];
        int argc = 0;

        char *token = strtok(buf, " ");
        while (token != NULL && argc < MAXARGS - 1) {
            args[argc++] = token;
            token = strtok(NULL, " ");
        }
        args[argc] = NULL;

        /*
         * If command is "./countnames" and at least one file is provided,
         * spawn one child per file.
         */
        if (strcmp(args[0], "./countnames") == 0 && argc > 1) {

            /*
             * Fork once per file.
             * IMPORTANT: We fork all children first,
             * then wait after, to allow parallel execution.
             */
            for (int i = 1; i < argc; i++) {

                pid = fork();

                if (pid < 0) {
                    perror("fork error");
                    exit(1);
                }

                if (pid == 0) {
                    /* ===== CHILD PROCESS ===== */

                    /*
                     * Create output filenames using child PID
                     * Example:
                     *   1234.out
                     *   1234.err
                     */
                    char outFile[64];
                    char errFile[64];

                    sprintf(outFile, "%d.out", getpid());
                    sprintf(errFile, "%d.err", getpid());

                    /*
                     * Open output files
                     * O_CREAT  -> create if not exists
                     * O_TRUNC  -> overwrite if exists
                     * 0644     -> file permissions
                     */
                    int fd_out = open(outFile,
                                      O_WRONLY | O_CREAT | O_TRUNC,
                                      0644);

                    int fd_err = open(errFile,
                                      O_WRONLY | O_CREAT | O_TRUNC,
                                      0644);

                    if (fd_out < 0 || fd_err < 0) {
                        perror("open error");
                        exit(1);
                    }

                    /*
                     * Redirect stdout and stderr
                     * After this:
                     *   printf()  → PID.out
                     *   fprintf(stderr, ...) → PID.err
                     */
                    dup2(fd_out, STDOUT_FILENO);
                    dup2(fd_err, STDERR_FILENO);

                    close(fd_out);
                    close(fd_err);

                    /*
                     * Prepare argument list for execvp
                     * Each child runs:
                     *   ./countnames file_i
                     */
                    char *child_args[3];
                    child_args[0] = "./countnames";
                    child_args[1] = args[i];
                    child_args[2] = NULL;

                    execvp(child_args[0], child_args);

                    /* If exec fails */
                    perror("exec error");
                    exit(1);
                }
            }

            /*
             * ===== PARENT PROCESS =====
             * Wait for all children to finish.
             * Using wait() allows any child to finish first,
             * preventing bottlenecks.
             */
            for (int i = 1; i < argc; i++) {

                pid_t finished_pid = wait(&status);

                if (finished_pid < 0) {
                    perror("wait error");
                    continue;
                }

                /*
                 * Optional: print exit information
                 * (good practice and sometimes expected)
                 */
                if (WIFEXITED(status)) {
                    printf("Process %d exited with status %d\n",
                           finished_pid,
                           WEXITSTATUS(status));
                }
                else if (WIFSIGNALED(status)) {
                    printf("Process %d terminated by signal %d\n",
                           finished_pid,
                           WTERMSIG(status));
                }
            }
        }
        else {
            printf("Unsupported command\n");
        }

        /* Reprint shell prompt */
        printf("%% ");
        fflush(stdout);
    }

    return 0;
}