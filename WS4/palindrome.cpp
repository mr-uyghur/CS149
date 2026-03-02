#include <stdio.h>
#include <string.h>

int main(void) {
    char input[51];
    char cleaned[51];
    int j = 0;

    // Read input line
    fgets(input, sizeof(input), stdin);

    // Remove trailing newline
    size_t len = strlen(input);
    if (len > 0 && input[len - 1] == '\n') {
        input[len - 1] = '\0';
        len--;
    }

    // Remove spaces
    for (int i = 0; i < len; i++) {
        if (input[i] != ' ') {
            cleaned[j++] = input[i];
        }
    }
    cleaned[j] = '\0';

    // Check palindrome
    int isPalindrome = 1;
    int left = 0;
    int right = j - 1;

    while (left < right) {
        if (cleaned[left] != cleaned[right]) {
            isPalindrome = 0;
            break;
        }
        left++;
        right--;
    }

    if (isPalindrome) {
        printf("palindrome: %s\n", input);
    } else {
        printf("not a palindrome: %s\n", input);
    }

    return 0;
}