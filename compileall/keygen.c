#include <stdio.h>
#include <stdlib.h>
#include <time.h>

// Function to generate and print a random key of specified length
void generateKey(int length) {
    const char charset[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ "; // 27 characters
    const int charsetSize = sizeof(charset) - 1; // Exclude the null terminator

    //random number generator
    srand(time(NULL));

    for (int i = 0; i < length; i++) {
        int keyIndex = rand() % charsetSize; // Generate a random index
        printf("%c", charset[keyIndex]); // Print the character at the random index
    }

    // End with a newline character
    printf("\n");
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s keylength\n", argv[0]);
        return 1;
    }

    int keyLength = atoi(argv[1]); // Convert argument to integer

    if (keyLength <= 0) {
        fprintf(stderr, "Key length must be a positive integer\n");
        return 1;
    }

    generateKey(keyLength);

    return 0;
}
