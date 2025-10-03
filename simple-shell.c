#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>      
#include <sys/wait.h>
#include <sys/time.h>
#include <errno.h>
#include <string.h>
#include <signal.h>

typedef struct {
	char comm[1024];
	pid_t pid;
	struct timeval start;
	struct timeval end;
}history_logs;

history_logs logs[100];
int n=0;

void terminate(int sig){
	printf("\n\n===== Command History =====\n");
	for (int i=0; i<n; i++){
		long st=logs[i].start.tv_sec*1000+logs[i].start.tv_usec/1000;
		long end=logs[i].end.tv_sec * 1000 + logs[i].end.tv_usec / 1000;
		long tim=end-st;

		printf("[%d] PID=%d | \"%s\" | Start=%ld ms | Duration=%ld ms\n",i + 1,logs[i].pid,logs[i].comm,st,tim);
    }
	if (sig == SIGINT){
        printf(" Bye Byee.. (;;)\n");
        exit(0);
    } else {
        printf("===========================\n");
    }
}

int main(int argc, char* argv[]){
	if (argc!=3){
		fprintf(stderr,"Missing: <NCPU> <TSLICE(ms)>\n");
		exit(1);
	}

	int ncpu=atoi(argv[1]);
	int tslice=atoi(argv[2]);
	printf("NCPU=%d, TSLICE=%d ms\n",ncpu,tslice);

	char* line=NULL;
	size_t line_len=0;

	signal(SIGINT,terminate);

	while(1){
		printf("arzl@donkey:??");
		
		if (getline(&line, &line_len, stdin) == -1) {
	        perror("getline");
	        exit(1);
	    }

	    history_logs h;
	    char line_og[1024];
		strncpy(line_og, line, sizeof(line_og));
		line_og[sizeof(line_og)-1] = '\0';
		strcpy(h.comm, line_og); 

		char* coms[10];
		int ncom=0;

		char* part= strtok(line,"|");
		while (part!=NULL){
			coms[ncom++]=part;
			part=strtok(NULL,"|");
		}

		char* args[10][100];
		int argc[10];

		for (int i=0;i<ncom;i++){
			argc[i]=0;
			char* comm=strtok(coms[i], " \t\n");
			while (comm!=NULL){
				args[i][argc[i]++]=comm;
				comm=strtok(NULL, " \t\n");
			}
			args[i][argc[i]]=NULL;
		}

		if (args[0][0] == NULL) continue;
		if (strcmp(args[0][0], "history") == 0) {
		    terminate(0);
		    continue;
		}

		int pipes[ncom-1][2];
		pid_t pids[ncom];
		for(int i=0;i<ncom-1;i++){
			if(pipe(pipes[i]) == -1){
				perror("pipe");
				exit(1);
			}
		}

		gettimeofday(&h.start,NULL);


		for(int i=0;i<ncom;i++){
			pids[i] = fork();


			if (pids[i] == 0){
				if (i>0){
					dup2(pipes[i-1][0],STDIN_FILENO);
				}

				if(i<ncom-1){
					dup2(pipes[i][1],STDOUT_FILENO);
				}

				for(int j=0;j<ncom-1;j++){
					close(pipes[j][0]);
					close(pipes[j][1]);
				}
				if(args[i][0]!=NULL){
					execvp(args[i][0],args[i]);
					perror("execvp");
					exit(1);
				}	
			}

		}

		for (int i = 0; i < ncom - 1; i++) {
	    	close(pipes[i][0]);
	        close(pipes[i][1]);
	    }

	    int last_status = 0;
	    for (int i = 0; i < ncom; i++) {
	        int st = 0;
	        if (waitpid(pids[i], &st, 0) == -1) perror("waitpid");
	        if (i == ncom - 1) last_status = st;
	    }

	    gettimeofday(&h.end,NULL);
	    h.pid = pids[ncom - 1];
	    logs[n++]=h;

	}

	free(line);
	return 0;
}