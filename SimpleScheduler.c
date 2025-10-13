#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/time.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <fcntl.h>
#define MAXJOBS 100
#define QCAP    100

typedef struct { pid_t pid; char name[100]; } sched_msg_t; // for shell to scheduler
typedef struct { pid_t pid; int compt; int waitt; } shell_msg_t; // for scheduler to shell

typedef struct {
    int   in_use;
    pid_t pid;
    char  name[100];

    int   start_q;     //starting timeslice
    int   end_q;       //final timeslice
    int   run_ticks;   //running timeslice
    int   running;   
} job_t;

static job_t jobs[MAXJOBS];

//queue for slots for jobs
static int rq[QCAP], rq_f=0, rq_r=0, rq_n=0;
static void rq_add(int slot){ if (rq_n<QCAP){ rq[rq_r]=slot; rq_r=(rq_r+1)%QCAP; rq_n++; } } //adding to ready queue
static int  rq_pop(int* slot){ if (rq_n<1) return 0; *slot=rq[rq_f]; rq_f=(rq_f+1)%QCAP; rq_n--; return 1; }//removing from ready queue
static int *run_slots = NULL;
static volatile sig_atomic_t tick_flag = 0;
static volatile sig_atomic_t want_shutdown = 0; //for shutdown
static int cur_tick = 0;
static int NCPU = 1;
static int FD_IN = 3; 
static int FD_OUT = 4;

//some signals
static void on_tick(int sig){ (void)sig; tick_flag = 1; }
static void on_shell_exit(int sig){ (void)sig; want_shutdown = 1; }

//slots allocation n shi....
static int alloc_slot(void){
    for (int i=0;i<MAXJOBS;i++){
        if (!jobs[i].in_use){
            jobs[i].in_use = 1;
            jobs[i].pid = -1;
            jobs[i].name[0] = '\0';
            jobs[i].start_q = 0;
            jobs[i].end_q = -1;
            jobs[i].run_ticks = 0;
            jobs[i].running = 0;
            return i;
        }
    }
    return -1;
}
static void free_slot(int slot){ if (slot>=0 && slot<MAXJOBS) jobs[slot].in_use = 0; }


static int submit_eof = 0; // for setting when FD_IN reaches EOF lol 
static void drain_new_jobs(void){
    while(1){
        sched_msg_t m;
        ssize_t nread = read(FD_IN, &m, sizeof m);
        if (nread == (ssize_t)sizeof m){
            int slot = alloc_slot();
            if (slot < 0) {
                continue; // queue is full :c
            }
            jobs[slot].pid = m.pid;
            snprintf(jobs[slot].name, sizeof jobs[slot].name, "%s", m.name);
            jobs[slot].start_q = cur_tick + 1; //updating start timeslice for job
            jobs[slot].end_q   = -1;
            rq_add(slot);
            continue;}
        if (nread == 0){ submit_eof = 1; break; }            // shell closed write end 
        if (nread == -1 && errno == EINTR) continue;         // interrupted error
        if (nread == -1 && (errno == EAGAIN || errno == EWOULDBLOCK)) break;
        break;
    }
}

// to check if process is alive
static int proc_alive(pid_t p){
    if (p <= 0) return 0;
    if (kill(p, 0) == 0) return 1;
    return (errno != ESRCH) && (errno != EINVAL); // error check
}

static void stop_slot(int slot){
    pid_t p = jobs[slot].pid;
    if (p <= 0) return;
    if (kill(p, SIGSTOP) == -1 && errno == ESRCH) { //sending sigstop to process
    }
    jobs[slot].running = 0;
}
static void cont_slot(int slot){
    pid_t p = jobs[slot].pid;
    if (p <= 0) return;
    if (kill(p, SIGCONT) == -1 && errno == ESRCH) { //sending sigcont to process
        jobs[slot].running = 0;
        return;
    }
    jobs[slot].running = 1;
}
// to send result to shell
static void send_result(int slot){
    if (!jobs[slot].in_use) return;
    if (jobs[slot].end_q < 0) {
        jobs[slot].end_q = cur_tick;
    }
    int comp_slices = jobs[slot].end_q - jobs[slot].start_q;
    if (comp_slices < 1) comp_slices = 1;
    int wait_slices = comp_slices - jobs[slot].run_ticks;
    if (wait_slices < 0) wait_slices = 0;
    shell_msg_t res = {
        .pid   = jobs[slot].pid,
        .compt = comp_slices,
        .waitt = wait_slices
    };



    (void)write(FD_OUT, &res, sizeof res);
    free_slot(slot);
}

static int all_done(void){
    if (rq_n > 0) return 0;
    for (int i=0;i<NCPU;i++) if (run_slots[i] >= 0) return 0; //finishing up stuff
    for (int i=0;i<MAXJOBS;i++) if (jobs[i].in_use) return 0;
    return 1;
}

int main(int argc, char **argv){
    if (argc != 3 && argc != 5){
        fprintf(stderr, "Usage: scheduler <NCPU> <TSLICE(ms)> [FD_IN FD_OUT]\n");
        return 1;
    }
    NCPU = atoi(argv[1]);
    int tslice = atoi(argv[2]);
    if (NCPU < 1 || tslice <= 0) {
        fprintf(stderr, "Bad arguments given\n"); //error checking
        return 1;
    }
    int fl = fcntl(FD_IN, F_GETFL, 0); //nonblock fds
    if (fl != -1) fcntl(FD_IN, F_SETFL, fl | O_NONBLOCK);
    //all signals
    struct sigaction sa; memset(&sa,0,sizeof sa);
    sigemptyset(&sa.sa_mask);
    sa.sa_handler = on_tick;       sigaction(SIGALRM, &sa, NULL);
    sa.sa_handler = on_shell_exit; sigaction(SIGUSR1, &sa, NULL);
    //all timer
    struct itimerval timer;
    timer.it_interval.tv_sec=tslice/1000;
    timer.it_interval.tv_usec=(tslice%1000)*1000;
    timer.it_value=timer.it_interval;
    setitimer(ITIMER_REAL, &timer, NULL);

    run_slots = calloc((size_t)NCPU, sizeof(int));
    if (!run_slots){ perror("calloc"); return 1;}  //error checking 
    for (int i=0;i<NCPU;i++) run_slots[i] = -1;

    //getting new jobs 
    drain_new_jobs();

    while(1){
        //again checking for new jobs to be safe
        drain_new_jobs();

        if (!tick_flag) { // sleeping until next SIGALRM signal
            pause();
            continue;
        }

        tick_flag = 0;
        cur_tick++;
        for (int i=0;i<NCPU;i++){
            int slot = run_slots[i];
            if (slot < 0) continue; // running the job

            //count of runnign timeslices
            jobs[slot].run_ticks++;

            //checking if process is alive.
            if (!proc_alive(jobs[slot].pid)) {
                jobs[slot].end_q = cur_tick;
                run_slots[i] = -1;
                send_result(slot);
                continue;
            }
            //removing the slot after process outed from runqueue
            stop_slot(slot);
            rq_add(slot);
            run_slots[i] = -1;
        }

        //run upto ncpu slots
        for (int i=0;i<NCPU;i++){
            if (run_slots[i] >= 0) continue;
            int slot;
            int attempts = rq_n; 
            while (attempts-- > 0 && rq_pop(&slot)){
                if (jobs[slot].start_q > cur_tick) {
                    rq_add(slot);
                    continue;
                }
                cont_slot(slot);
                if (jobs[slot].running) {
                    run_slots[i] = slot;
                    break;
                } else {
                    jobs[slot].end_q = cur_tick;
                    send_result(slot);
                }
            }
        }
        //exit when no jobs or exit called
        if ( (want_shutdown || submit_eof) && all_done() ) {break;}
    }
    free(run_slots);
    return 0;
}
