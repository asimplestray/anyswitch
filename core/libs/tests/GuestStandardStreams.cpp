#include <cstdio>
#include <cstring>
#include <cstdlib>

int main() {
    // Basic libc functionality test
    char buffer[256];
    snprintf(buffer, sizeof(buffer), "%d + %d = %d", 1, 2, 3);

    if (strcmp(buffer, "1 + 2 = 3") != 0) {
        fprintf(stderr, "Test failed: got '%s'\n", buffer);
        return 1;
    }

    // Test memory allocation
    void* ptr = malloc(1024);
    if (!ptr) {
        fprintf(stderr, "malloc failed\n");
        return 1;
    }
    memset(ptr, 0xAB, 1024);
    free(ptr);

    printf("All libc tests passed\n");
    return 0;
}
