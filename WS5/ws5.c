#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>

int main(void) {
    fork();
    fork();
    printf("hello world from PID %d!\n", getpid());
    return 0;
}