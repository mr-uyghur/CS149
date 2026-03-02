#include <stdio.h>
#include <string.h>
#include <ctype.h>

void RemoveNonAlpha(char userString[], char userStringAlphaOnly[]) {
    int j = 0;

    for (int i = 0; userString[i] != '\0'; i++) {
        if (isalpha(userString[i])) {
            userStringAlphaOnly[j++] = userString[i];
        }
    }

    userStringAlphaOnly[j] = '\0';  // Null terminate result
}

int main(void) {
    char userString[51];
    char userStringAlphaOnly[51];

    fgets(userString, sizeof(userString), stdin);

    // Remove trailing newline if present
    size_t len = strlen(userString);
    if (len > 0 && userString[len - 1] == '\n') {
        userString[len - 1] = '\0';
    }

    RemoveNonAlpha(userString, userStringAlphaOnly);

    printf("%s\n", userStringAlphaOnly);

    return 0;
}