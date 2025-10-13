#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <errno.h>
#include <string.h>
#include <signal.h>
#include <fcntl.h>

typedef struct {
    char name[100];
    pid_t pid;
    int start_t;
    int end_t;
    int wait_t;
    int status;
} job_info;

typedef struct {
    pid_t pid;
    char  name[100]; //struct to send to scheduler
} sched_msg_t;

typedef struct{
    pid_t pid;
    int compt;
    int waitt; //struct to get from scheduler
} shell_msg_t;

job_info jobs[100];
int job_count = 0;

int toSched_w = -1;
int toShell_r = -1;
pid_t sched_pid = -1;

static void handle_sigchld(int sig) {
    (void)sig;
    int saved_errno = errno;
    while (1) {  //sigchld handler
        int status;
        pid_t p = waitpid(-1, &status, WNOHANG);
        if (p <= 0) break;
    }
    errno = saved_errno;
}

int main(int argc, char* argv[]){
    if (argc!=3){
        fprintf(stderr,"Missing: <NCPU> <TSLICE(ms)>\n");
        exit(1);
    }
    int ncpu=atoi(argv[1]);
    int tslice=atoi(argv[2]);
    printf("NCPU=%d, TSLICE=%d ms\n",ncpu,tslice);

    int toSched[2], toShell[2];
    if (pipe(toSched) == -1) { perror("pipe toSched"); exit(1); }
    if (pipe(toShell) == -1) { perror("pipe toShell"); exit(1); }

    signal(SIGPIPE, SIG_IGN); //just ignore sigpipe to avoid problems with pipe

    //creating handler for sigchld
    struct sigaction sa_chld = {0};
    sa_chld.sa_handler = handle_sigchld;
    sigemptyset(&sa_chld.sa_mask);
    sa_chld.sa_flags = SA_RESTART;
    sigaction(SIGCHLD, &sa_chld, NULL);

    sched_pid = fork();
    if (sched_pid < 0) {
        perror("fork failed");
        exit(1);
    } else if (sched_pid == 0) {
        fcntl(toSched[0], F_SETFD, 0);   // read from shell submissions
        fcntl(toShell[1], F_SETFD, 0);   // write results back to shell

        char fd_in_str[16], fd_out_str[16];
        snprintf(fd_in_str,  sizeof fd_in_str,  "%d", toSched[0]); // read-end
        snprintf(fd_out_str, sizeof fd_out_str, "%d", toShell[1]); // write-end

        //Close unused ends in the child before exec 
        close(toSched[1]);
        close(toShell[0]);

        char *jazl[] = {"SimpleScheduler",argv[1],argv[2],fd_in_str,fd_out_str,NULL};
        execv("./SimpleScheduler", jazl);
        perror("Failed to execute Scheduler");
        _exit(1);
    }

    //parent
    close(toSched[0]);    
    close(toShell[1]);
    toSched_w = toSched[1];
    toShell_r = toShell[0];

    // Set non-blocking
    int flags = fcntl(toShell_r, F_GETFL, 0);
    if (flags != -1) fcntl(toShell_r, F_SETFL, flags | O_NONBLOCK);

    printf("Scheduler PID= %d\n",sched_pid);

    char* line=NULL;
    size_t line_len=0;

    while(1){
        printf("arzl@donkey:?? ");
        fflush(stdout);
        ssize_t nread = getline(&line, &line_len, stdin);
        if (nread == -1) {
            if (feof(stdin)) {
                printf("\nReceived EOF — exiting.\n");
                char *tmp = realloc(line, 5);
                if (!tmp) { perror("realloc"); break; }
                line = tmp;
                memcpy(line, "exit", 5);
            } else {
                perror("getline");
                continue;
            }
        } else {
            if (nread>0 && line[nread-1]=='\n') line[nread-1]='\0';
        }
        if (strncmp(line,"submit ",7)==0){
            char *exec_obj = line+7;
            if (*exec_obj == '\0') {
                fprintf(stderr, "submit requires an argument\n");
                continue;
            }

            pid_t job_pid = fork();
            if (job_pid < 0) {
                perror("fork");
                continue;
            }

            if (job_pid == 0){
                signal(SIGPIPE, SIG_DFL);
                signal(SIGUSR1, SIG_IGN);
                raise(SIGSTOP);                              //stopping the child process
                char* arul[]={exec_obj,NULL};
                execvp(exec_obj, arul);
                perror("Failed to execute object");
                _exit(1);
            } else {
                //recording by parent
                if (job_count < (int)(sizeof jobs / sizeof jobs[0])) {
                    strncpy(jobs[job_count].name, exec_obj, sizeof jobs[job_count].name - 1);
                    jobs[job_count].name[sizeof jobs[job_count].name - 1] = '\0';
                    jobs[job_count].pid = job_pid;
                    jobs[job_count].start_t = -1;
                    jobs[job_count].status = 0;
                    jobs[job_count].end_t = 0;
                    jobs[job_count].wait_t = 0;
                    job_count++;
                } else {
                    fprintf(stderr, "Job table full — cannot accept more jobs.\n");
                }

                sched_msg_t m;
                m.pid=job_pid;
                strncpy(m.name,exec_obj,sizeof m.name-1);
                m.name[sizeof m.name-1]='\0';
                ssize_t w=write(toSched_w,&m,sizeof m);
                if(w!=(ssize_t)sizeof m){
                    if(w==-1)perror("write to scheduler");
                    else fprintf(stderr,"Partial write to scheduler pipe\n");}  //error handling
                printf("Added Job: %s, PID=%d\n",exec_obj,job_pid);
            }

        }else if(strcmp(line,"exit")==0){ //handling exit command
            if (toSched_w != -1) { close(toSched_w); toSched_w = -1; }

            //Signalling scheduler that shell told to exit
            kill(sched_pid, SIGUSR1);
            //wait for it to exit
            waitpid(sched_pid, NULL, 0);

            //readign rest of messages from scheduler for last message to print results.
            shell_msg_t msg;
            ssize_t r;
            while((r=read(toShell_r,&msg,sizeof(msg)))>0){
                if(r==(ssize_t)sizeof(msg)){
                    for(int i=0;i<job_count;i++){
                        if (jobs[i].pid==msg.pid){
                            jobs[i].end_t=msg.compt;
                            jobs[i].wait_t=msg.waitt;
                            break;
                        }
                    }
                }
            }
            if (r == -1 && errno != EAGAIN && errno != EWOULDBLOCK) perror("read from scheduler"); //error handling

            //Report printing
            printf("Reports:\n");
            printf("%-20s %-8s %-15s %-10s\n", "Name", "PID", "Completion(ms)", "Wait(ms)");
            printf("-----------------------------------------------------------------\n");
            for (int i = 0; i < job_count; i++) {
                int comp = (jobs[i].end_t > 0 ? jobs[i].end_t : 1) ;
                int wait = jobs[i].wait_t ;
                char comp_str[32], wait_str[32];
                sprintf(comp_str, "%d x %dms", comp, tslice);
                sprintf(wait_str, "%d x %dms", wait, tslice);
                printf("%-20s %-8d %-15s %-10s\n", jobs[i].name, jobs[i].pid, comp_str, wait_str);            }
            break;
        } else {
            printf("Unknown command: %s\n", line);
        }
    }
    free(line);
    if (toShell_r != -1) close(toShell_r);
    return 0;
}
