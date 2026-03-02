#include <stdio.h>
#include <string.h>

int main(void) {
    char input[51];

    while (1) {
        fgets(input, sizeof(input), stdin);

        // Remove trailing newline if present
        size_t len = strlen(input);
        if (len > 0 && input[len - 1] == '\n') {
            input[len - 1] = '\0';
            len--;
        }

        // Check termination conditions
        if (strcmp(input, "Done") == 0 ||
            strcmp(input, "done") == 0 ||
            strcmp(input, "d") == 0) {
            break;
        }

        // Print reversed string
        for (int i = len - 1; i >= 0; i--) {
            printf("%c", input[i]);
        }
        printf("\n");
    }

    return 0;
}