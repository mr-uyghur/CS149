// Write a program that reads a list of integers, and outputs whether the list contains all multiples of 10, no multiples of 10, or mixed values. Define a function named IsArrayMult10 that takes an array as a parameter, representing the list, and an integer as a parameter, representing the size of the list. IsArrayMult10() returns a boolean that represents whether the list contains all multiples of ten. Define a function named IsArrayNoMult10 that takes an array as a parameter, representing the list, and an integer as a parameter, representing the size of the list. IsArrayNoMult10() returns a boolean that represents whether the list contains no multiples of ten.

// Then, write a main program that takes an integer, representing the size of the list, followed by the list values. The first integer is not in the list. Assume that the list will always contain less than 20 integers.

// Ex: If the input is:

// 5 20 40 60 80 100
// the output is:

// all multiples of 10

// Ex: If the input is:

// 5 11 -32 53 -74 95
// the output is:

// no multiples of 10

// Ex: If the input is:

// 5 10 25 30 40 55
// the output is:

// mixed values

// The program must define and call the following two functions. IsArrayMult10 returns true if all integers in the array are multiples of 10 and false otherwise. IsArrayNoMult10 returns true if no integers in the array are multiples of 10 and false otherwise.
// bool IsArrayMult10(int inputVals[], int numVals)
// bool IsArrayNoMult10(int inputVals[], int numVals)

#include <stdio.h>
#include <stdbool.h>

/* Returns true if all values in the array are multiples of 10 */
bool IsArrayMult10(int inputVals[], int numVals) {
   int i;

   for (i = 0; i < numVals; i++) {
      if (inputVals[i] % 10 != 0) {
         return false;
      }
   }

   return true;
}

/* Returns true if no values in the array are multiples of 10 */
bool IsArrayNoMult10(int inputVals[], int numVals) {
   int i;

   for (i = 0; i < numVals; i++) {
      if (inputVals[i] % 10 == 0) {
         return false;
      }
   }

   return true;
}

int main(void) {
   int numVals;
   int inputVals[20];
   int i;

   scanf("%d", &numVals);

   for (i = 0; i < numVals; i++) {
      scanf("%d", &inputVals[i]);
   }

   if (IsArrayMult10(inputVals, numVals)) {
      printf("all multiples of 10\n");
   }
   else if (IsArrayNoMult10(inputVals, numVals)) {
      printf("no multiples of 10\n");
   }
   else {
      printf("mixed values\n");
   }

   return 0;
}