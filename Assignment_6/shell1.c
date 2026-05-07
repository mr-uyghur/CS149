/*
 * shell.c - Assignment 6
 *
 * Author names: Frederick Nguyen | Ali Halmamat
 * Author emails: quangnguyen1705@gmail.com | ali.Halmamat@sjsu.edu
 * Last modified date: 04/17/2026
 * Creation date: 04/17/2026
 *
 * A simple shell that executes the multi-threaded countnames program.
 *
 * Usage: ./shell <file1> [file2 ...]
 * The shell exec's countnames with the provided file arguments.
 *
 * Compile:
 *   gcc -o shell shell.c -Wall -Werror
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

int main(int argc, char *argv[])
{
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <file1> [file2 ...]\n", argv[0]);
        return EXIT_FAILURE;
    }

    /* Build the argument list for execv:
     * argv[0] = "./countnames", argv[1..argc-1] = the input files, NULL terminator
     */
    int num_args = argc;          /* "./countnames" + (argc-1) files */
    char **exec_args = malloc((num_args + 1) * sizeof(char *));
    if (exec_args == NULL) {
        perror("malloc");
        return EXIT_FAILURE;
    }

    exec_args[0] = "./countnames";
    for (int i = 1; i < argc; i++) {
        exec_args[i] = argv[i];
    }
    exec_args[num_args] = NULL;

    /* Fork a child process to exec countnames */
    pid_t pid = fork();
    if (pid < 0) {
        perror("fork");
        free(exec_args);
        return EXIT_FAILURE;
    }

    if (pid == 0) {
        /* Child: exec countnames */
        execv("./countnames", exec_args);
        /* execv only returns on error */
        perror("execv");
        free(exec_args);
        exit(EXIT_FAILURE);
    }

    /* Parent: wait for child to finish */
    int status;
    waitpid(pid, &status, 0);

    free(exec_args);

    if (WIFEXITED(status)) {
        return WEXITSTATUS(status);
    }
    return EXIT_FAILURE;
}