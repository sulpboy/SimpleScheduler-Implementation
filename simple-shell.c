#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>      
#include <sys/wait.h>
#include <sys/time.h>
#include <errno.h>
#include <string.h>
#include <signal.h>

typedef struct {
    char name[100];
    pid_t pid;
    int start_t;      
    int end_t;         
    int wait_t;       
    int status;
} job_info;

job_info jobs[100];
int job_count = 0;

int main(int argc, char* argv[]){
	if (argc!=3){
		fprintf(stderr,"Missing: <NCPU> <TSLICE(ms)>\n");
		exit(1);
	}

	int ncpu=atoi(argv[1]);
	int tslice=atoi(argv[2]);
	printf("NCPU=%d, TSLICE=%d ms\n",ncpu,tslice);

	pid_t sched_pid= fork();

	if (sched_pid<0){
		perror("fork failed");
		exit(1);
	} else if (sched_pid==0){
		char *jazl[]={"SimpleScheduler",argv[1],argv[2],NULL};
		execv("./SimpleScheduler",jazl);
		perror("Failed to execute Scheduler");
		exit(1);
	} 

	printf("Scheduler PID= %d\n",sched_pid);
	fflush(stdout);

	char* line=NULL;
	size_t line_len=0;

	while(1){
		printf("arzl@donkey:??");
		
		if (getline(&line, &line_len, stdin) == -1) {
	        perror("getline");
	        exit(1);
	    }

	   	line[strcspn(line,"\n")]=0;

	   	if (strncmp(line,"submit ",7)==0){
	   		char *exec_obj=line+7;
	   		pid_t job_pid=fork();

	   		if (job_pid==0){
	   			raise(SIGSTOP);
	   			char* arul[]={exec_obj,NULL};
	   			execvp(exec_obj,arul);
	   			perror("Failed to execute object");
	   			exit(1);
	   		} else {
	   			strcpy(jobs[job_count].name, exec_obj);
	   			jobs[job_count].pid=job_pid;
	   			jobs[job_count].start_t=-1;
	   			jobs[job_count].status=0;
	   			jobs[job_count].end_t = -1;
                jobs[job_count].wait_t = 0;
	   			job_count++;
	   			printf("Added Job: %s, PID=%d\n",exec_obj,job_pid);
	   		}
	   	} else if (strcmp(line,"exit")==0){
	   		kill(sched_pid,SIGUSR1);
	   		waitpid(sched_pid,NULL,O);
	   		printf("Reports:\n");
	   		printf("%-15s %-8s %-15s %-10s\n", "Name", "PID", "Completion(ms)", "Wait(ms)");
			printf("--------------------------------------------------------\n");
	   		for (int i = 0; i < job_count; i++) {
	   			int comp=(jobs[i].end_t>0 ? jobs[i].end_t:1)*tslice;
	   			int wait=jobs[i].wait_t*tslice;
	   			printf("%-15s %-8d %-15d %-10d\n",jobs[i].name, jobs[i].pid,comp,wait);
	   		}
	   		break;
	   	} else {
	   		printf("Unknown command: %s\n", line);
	   	}
	}

	free(line);
	return 0;
}