#endif
#ifndef NREPS
#define NREPS 1           // repeat count; bump with -DNREPS=...
#endif

static uint64_t fib(uint32_t n) {
    if (n < 2) return n;
    return fib(n - 1) + fib(n - 2);
}

int main(int argc, char **argv) {
    (void)argc; (void)argv;
    printf("[fib20] start pid=%d, N=%d, REPS=%d\n", getpid(), N, NREPS);
    fflush(stdout);

    volatile uint64_t acc = 0;   // prevent optimization
    for (int r = 0; r < NREPS; ++r) acc ^= fib(N);

    printf("[fib20] done pid=%d, result=%llu\n",
           getpid(), (unsigned long long)acc);
    fflush(stdout);
    return 0;
}
