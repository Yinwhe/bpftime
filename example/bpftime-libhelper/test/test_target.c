#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main() {
    printf("Test target started with PID: %d\n", getpid());
    
    while (1) {
        // 定期调用 malloc 以便监控
        void *ptr = malloc(1024);
        if (ptr) {
            printf("Allocated memory at %p\n", ptr);
            free(ptr);
        }
        sleep(2);
    }
    
    return 0;
}