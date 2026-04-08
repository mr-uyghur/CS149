/*
 * q4.c
 *
 * Question: Write a simple program that allocates memory using malloc() 
 * but forgets to free it before exiting. What happens when this program 
 * runs? Can you use gdb to find any problems with it? How about valgrind 
 * (again with the --leak-check=yes flag)?
 *
 * Answers:
 * 1. What happens when this program runs?
 *    The program runs normally and exits without any errors or output. 
 *    The operating system reclaims all memory allocated by the process 
 *    upon process termination, so the leak does not persist after exit.
 * 
 * 2. Can you use gdb to find any problems with it?
 *    No. GDB does not track memory allocations or detect memory leaks. 
 *    It will simply show that the program executed and exited normally.
 * 
 * 3. How about valgrind (with the --leak-check=yes flag)?
 *    Valgrind will successfully detect the problem. At the end of execution, 
 *    valgrind will print a heap summary pointing out that memory was lost 
 *    ("definitely lost" or "still reachable"), specifically identifying 
 *    the file and line number where the leaked memory was allocated via malloc().
 */

#include <stdio.h>
#include <stdlib.h>

int main(void) {
    // Allocate memory for an integer pointer
    int *ptr = (int *)malloc(sizeof(int));
    *ptr = 42;

    // Forget to free the allocated memory!
    // free(ptr);

    return 0;
}
