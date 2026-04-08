/*
 * q5.c
 *
 * Question: Write a program that creates an array of integers called data 
 * of size 100 using malloc; then, set data[100] to zero. What happens 
 * when you run this program? What happens when you run this program 
 * using valgrind? Is the program correct?
 *
 * Answers:
 * 1. What happens when you run this program?
 *    The program will likely run and exit silently without crashing, 
 *    even though it performs a memory bounds violation. This is because 
 *    writing slightly past the end of a malloc'd block often just overwrites 
 *    adjacent heap metadata or padding, rather than causing an immediate 
 *    segmentation fault. However, the behavior is undefined.
 *
 * 2. What happens when you run this program using valgrind?
 *    Valgrind will catch the out-of-bounds error and report an "Invalid 
 *    write of size 4" (assuming a 4-byte integer) on the exact line where 
 *    we set data[100] = 0. It will indicate that the write occurred just 
 *    past the end of the block we allocated. (It will also report a memory 
 *    leak since we did not free the memory).
 *
 * 3. Is the program correct?
 *    No, the program is completely incorrect. An array of size 100 has 
 *    valid indices ranging from 0 to 99 strictly. Trying to access data[100] 
 *    is an out-of-bounds access and constitutes undefined behavior in C.
 */

#include <stdio.h>
#include <stdlib.h>

int main(void) {
    // Create an array of integers of size 100 using malloc
    int *data = (int *)malloc(100 * sizeof(int));

    // Set the 101st element to zero (out-of-bounds access)
    data[100] = 0;

    // Optional: free the memory (valgrind would complain if omitted)
    free(data);

    return 0;
}
