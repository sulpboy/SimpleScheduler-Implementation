#include "dummy_main.h"

int main(int argc, char **argv) {
    for (int i = 0; i < 5; i++) {
        printf("[Job %d] Tick %d\n", getpid(), i+1);
        fflush(stdout);
        usleep(100*1000); // 100ms
    }
    return 0;
}
