#ifndef DUMMY_MAIN_H
#define DUMMY_MAIN_H

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <sys/time.h>

int dummy_main(int argc, char **argv);

volatile sig_atomic_t start = 0;

void handle_start_signal(int sig) {
    start = 1;
}

void handle_stop_signal(int sig) {
    kill(getpid(), SIGSTOP);
}

int main(int argc, char **argv) {
    signal(SIGUSR1, handle_start_signal); 
    signal(SIGUSR2, handle_stop_signal);   

    printf("[Job %d] Ready, waiting for SIGUSR1 to start...\n", getpid());
    fflush(stdout);

    while (!start)
        pause();

    printf("[Job %d] Starting execution now!\n", getpid());
    fflush(stdout);

    int ret = dummy_main(argc, argv);

    printf("[Job %d] Finished execution.\n", getpid());
    fflush(stdout);

    return ret;
}

#define main dummy_main
#endif