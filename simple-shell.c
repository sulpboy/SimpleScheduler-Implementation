#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>      
#include <sys/wait.h>
#include <sys/time.h>
#include <errno.h>
#include <string.h>
#include <signal.h>


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

	printf("Scheduler PID= %d\n");
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
	   			printf("Added Job: %s, PID=%d\n",exec_obj,job_pid);
	   		}
	   	} else if (strncmp(line,"exit")==0){
	   		break;
	   	} else {
	   		printf("Unknown command: %s\n", line);
	   	}
	}

	free(line);
	return 0;
}