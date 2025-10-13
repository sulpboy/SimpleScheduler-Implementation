#ifndef DUMMY_MAIN_H
#define DUMMY_MAIN_H
#include <signal.h>
int dummy_main(int argc, char **argv);
int main(int argc, char **argv) {
    raise(SIGSTOP);  //sleeping the process
    //scheduler should sigcont the process to run it

    int ret = dummy_main(argc, argv);
    return ret;
}
#define main dummy_main
#endif
