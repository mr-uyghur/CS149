// Write a program that takes in a positive integer as input, and outputs a string of 1's and 0's representing the integer in binary. For an integer x, the algorithm is:

// As long as x is greater than 0
//    Output x % 2 (remainder is either 0 or 1)
//    x = x / 2
// Note: The above algorithm outputs the 0's and 1's in reverse order. You will need to write a second function to reverse the string.

// Ex: If the input is:

// 6
// the output is:

// 110

// The program must define and call the following two functions. Define a function named IntToReverseBinary that takes an int named integerValue as the first parameter and assigns the second parameter, binaryValue, with a string of 1's and 0's representing the integerValue in binary (in reverse order). Define a function named StringReverse that takes an input string as the first parameter and assigns the second parameter, reversedString, with the reverse of inputString.

// void IntToReverseBinary(int integerValue, char binaryValue[])
// void StringReverse(char inputString[], char reversedString[])

#include <stdio.h>

/* 
 * Converts an integer to binary in reverse order.
 * Example: 6 becomes "011" first.
 */
void IntToReverseBinary(int integerValue, char binaryValue[]) {
   int i = 0;  /* index for storing characters in the array */

   /* Keep dividing until the number becomes 0 */
   while (integerValue > 0) {
      /* Store the remainder (0 or 1) as a character */
      binaryValue[i] = (integerValue % 2) + '0';

      /* Update the integer by dividing by 2 */
      integerValue = integerValue / 2;
      i++;
   }

   /* Add null terminator to mark end of string */
   binaryValue[i] = '\0';
}

/* 
 * Reverses a string and stores the result in reversedString.
 * Example: "011" becomes "110".
 */
void StringReverse(char inputString[], char reversedString[]) {
   int length = 0;
   int i;

   /* Find the length of the input string */
   while (inputString[length] != '\0') {
      length++;
   }

   /* Copy characters from end of inputString to start of reversedString */
   for (i = 0; i < length; i++) {
      reversedString[i] = inputString[length - 1 - i];
   }

   /* Add null terminator at the end of reversed string */
   reversedString[length] = '\0';
}

int main(void) {
   int userNum;              /* stores the user input number */
   char reverseBinary[32];   /* stores binary digits in reverse order */
   char binary[32];          /* stores final correct binary string */

   /* Read integer input from user */
   scanf("%d", &userNum);

   /* Convert integer to reversed binary string */
   IntToReverseBinary(userNum, reverseBinary);

   /* Reverse the binary string to get correct order */
   StringReverse(reverseBinary, binary);

   /* Print the final binary representation */
   printf("%s\n", binary);

   return 0;  /* indicate successful program termination */
}