#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <time.h>
#include <errno.h>


typedef struct{
    char name[100];
    pid_t pid;
} job_st;

static volatile sig_atomic_t tick = 0;
volatile sig_atomic_t shell_status = 0;

void tickh(int sig){
    tick=1;
}

void shellexit(int sig){
    shell_status=1;
}

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
    
    signal(SIGALRM, tickh);
    signal(SIGUSR1,shellexit);

    struct itimerval timer;
    timer.it_interval.tv_sec=tslice/1000;
    timer.it_interval.tv_usec=(tslice%1000)*1000;
    timer.it_value=timer.it_interval;
    setitimer(ITIMER_REAL,&timer,NULL);
    

    pid_t run_jobs[ncpu];
    int cur_run=0;


    for(int i=0;i<ncpu;i++){
        pid_t p = pop();
        if(p == -1){
            break;
        }
        run_jobs[cur_run++]=p;
        kill(p,SIGCONT);
    }

    int cur_tick=0;
    while(1){
        pause();

        if(!tick) continue;
        tick = 0;
        cur_tick++;

        for(int i=0;i<ncpu;i++){
            pid_t p = run_jobs[i];
            if (p<=0) continue;

            if (kill(p, 0) == -1 && errno == ESRCH) {
                printf("Job PID=%d finished at tick=%d\n", p, cur_tick);
                run_jobs[i] = -1;
                continue;
            }

            if(kill(p,SIGSTOP) == -1){
                if( errno !=ESRCH){};
            }else{
                add(p,NULL);
            }
        }

        cur_run=0;

        for(int i=0;i<ncpu;i++){
            pid_t p =pop();
            if(p==-1) break;
            run_jobs[cur_run++] = p;
            kill(p,SIGCONT);
        }

        int das=0;
        for(int i=0;i<ncpu;i++){
            if(run_jobs[i]>0){
                das=1;
                break;
            }
        }

        if (shell_status && n==0 && !das){
            printf("Scheduler terminating\n");
            break;
        }

    }

    return 0;
}