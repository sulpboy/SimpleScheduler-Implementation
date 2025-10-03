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


	}

	free(line);
	return 0;
}