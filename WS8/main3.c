// Define a function named SortArray that takes an array of integers and the number of elements in the array as parameters. Function SortArray() modifies the array parameter by sorting the elements in descending order (highest to lowest). Then write a main program that reads a list of integers from input, stores the integers (starting from the second integer) in an array, calls SortArray(), and outputs the sorted array. The first input integer indicates how many numbers are in the list. Assume that the list will always contain less than 20 integers.

// Ex: If the input is:

// 5 10 4 39 12 2
// the output is:

// 39,12,10,4,2,

// For coding simplicity, follow every output value by a comma, including the last one.

// Your program must define and call the following function:
// void SortArray(int sortingList[], int numVals)

// Hint: Sorting an array can be done in many ways. You are welcome to look up and use any existing algorithm. Some believe the simplest to code is bubble sort: https://en.wikipedia.org/wiki/Bubble_sort. But you are welcome to try others: https://en.wikipedia.org/wiki/Sorting_algorithm.


#include <stdio.h>

/* Sorts the array in descending order */
void SortArray(int sortingList[], int numVals) {
   int i;
   int j;
   int temp;

   /* Bubble sort for highest to lowest */
   for (i = 0; i < numVals - 1; i++) {
      for (j = 0; j < numVals - 1 - i; j++) {
         if (sortingList[j] < sortingList[j + 1]) {
            temp = sortingList[j];
            sortingList[j] = sortingList[j + 1];
            sortingList[j + 1] = temp;
         }
      }
   }
}

int main(void) {
   int numVals;
   int sortingList[20];
   int i;

   scanf("%d", &numVals);

   for (i = 0; i < numVals; i++) {
      scanf("%d", &sortingList[i]);
   }

   SortArray(sortingList, numVals);

   for (i = 0; i < numVals; i++) {
      printf("%d,", sortingList[i]);
   }

   printf("\n");

   return 0;
}