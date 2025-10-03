#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <time.h>

typedef struct{
    char name[100];
    pid_t pid;
} job_st;

job_st rdqueue[100];
int f=0, r=0;
int n=0;

void add(pid_t pid, const char* name){
    if (n>=100) return;
    rdqueue[r].pid=pid;
    if (name){
        snprintf(rdqueue[r].name,100,"%s",name);
    }
    n++;
    r=(r+1)%100;
}

pid_t pop(){
    if (n<1) return -1;
    pid_t pid=rdqueue[f].pid;
    f=(f+1)%100;
    n--;
    return pid;
}


int main(int argc, char* argv[]){
    if (argc!=3){
        fprintf(stderr,"Missing: <NCPU> <TSLICE(ms)>\n");
        exit(1);
    }

    int ncpu=atoi(argv[1]);
    int tslice=atoi(argv[2]);
    printf("NCPU=%d, TSLICE=%d ms\n",ncpu,tslice);
}