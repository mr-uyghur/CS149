/*
 * q6.c
 *
 * Question: Create a program that allocates an array of integers (as above),
 * frees them, and then tries to print the value of one of the elements of 
 * the array. Does the program run? What happens when you use valgrind on it?
 *
 * Answers:
 * 1. Does the program run?
 *    Yes, the program typically runs and prints a value without crashing. 
 *    Although the memory has been freed, the operating system generally 
 *    does not unmap the heap page immediately, so accessing the freed pointer
 *    does not instantly cause a segmentation fault. The printed value may 
 *    be garbage or might just be whatever was previously there, but it 
 *    is fundamentally undefined behavior (a "use-after-free" bug).
 *
 * 2. What happens when you use valgrind on it?
 *    Valgrind specifically detects this bug and flags it as an "Invalid read 
 *    of size 4" exactly at the line where we dereference the array element. 
 *    Valgrind's report will vividly point out that we are trying to read 
 *    from a block of memory that has already been freed, even indicating 
 *    the exact program location where the free() occurred.
 */

#include <stdio.h>
#include <stdlib.h>

int main(void) {
    // Allocate an array of integers
    int *data = (int *)malloc(100 * sizeof(int));

    // Initialize an element so we can print it later
    data[50] = 42;

    // Free the allocated memory
    free(data);

    // Try to print the value of an element after it was freed
    printf("Value at index 50 after free: %d\n", data[50]);

    return 0;
}
