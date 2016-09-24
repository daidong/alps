#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <limits.h>
#include <string.h>

#include <mpi.h>

#include "utils.h"
#include "store.h"
#include "dbkey.h"

int static ALPS_DIVIDE = 2;

int main(int argc, char **argv){
	int rank;

	MPI_Init(&argc,&argv);
	MPI_Comm_rank(MPI_COMM_WORLD, &rank);

	
	if (rank < ALPS_DIVIDE){
		
		const char *fifo_name = "/tmp/test.binary"; //this must be a local file
		
		FILE *pipe_fd;
		size_t len = 0;
		ssize_t read;
		
		char *line = NULL;
		int bytes_read = 0;
		
		pipe_fd = fopen(fifo_name, "r");
		if (pipe_fd == NULL)
			exit(0);

		int child_pid, pid, fd, flag;
		long long timestamp;
		char execname[128], argstr[128], args[128], env[128], filename[256], retstr[128];
		
		struct alps_exec * all_alive_execs;

		while ((read = getline(&line, &len, pipe_fd)) != -1){
			bytes_read += len;
			char *p = (line + 1);

			switch ((*line)){
				case 'a':
					sscanf(p, "%d %lld %d\n", &child_pid, &timestamp, &pid);
					printf("Create Process - Child-PID: %d, timestamp: %lld, Curr-PID: %d\n", child_pid, timestamp, pid);

					break;
				case 'b':
					sscanf(p, "%d %lld\n", &pid, &timestamp);
					printf("Exit Process - PID: %d, timestamp: %lld\n", pid, timestamp);
					break;
				case 'c':
					sscanf(p, "%d %lld %s %s %s %s %s\n", &pid, &timestamp, execname, argstr, env, filename, args);
					printf("Execve Start - PID: %d, timestamp: %lld, execname: %s, argstr: %s, env: %s, filename: %s, args: %s\n", pid, timestamp, execname, argstr, env, filename, args);
					break;
				case 'd':
					sscanf(p, "%d %lld %s %s\n", &pid, &timestamp, retstr, env);
					printf("Execve Return - PID: %d, timestamp: %lld, retstr: %s, env: %s\n", pid, timestamp, retstr, env);
					break;
				case 'e':
					sscanf(p, "%d %lld %s %d %d\n", &pid, &timestamp, filename, &fd, &flag);
					printf("Open - PID: %d, timestamp: %lld, filename: %s, fd: %d, flag:%d\n", pid, timestamp, filename, fd, flag);
					break;
				case 'g':
					sscanf(p, "%d %lld %s %d\n", &pid, &timestamp, filename, &fd);
					printf("Close - PID: %d, timestamp: %lld, filename: %s, fd: %d\n", pid, timestamp, filename, fd);
					break;
				case 'p':
					sscanf(p, "%d %lld %s\n", &pid, &timestamp, filename);
					printf("Read - PID: %d, timestamp: %lld, filename: %s\n", pid, timestamp, filename);
					break;
				case 'q':
					sscanf(p, "%d %lld %s\n", &pid, &timestamp, filename);
					printf("Write - PID: %d, timestamp: %lld, filename: %s\n", pid, timestamp, filename);
					break;
			}

			int target;
			MPI_Send(line,strlen(line), MPI_CHAR, target, 0, MPI_COMM_WORLD);
		}
		
		fclose(pipe_fd);
	} 
	else {
		
		char buffer[256];
		MPI_Status status;
		MPI_Recv(buffer, 256, MPI_CHAR, MPI_ANY_SOURCE, 0, MPI_COMM_WORLD, &status);
		MPI_Send(buffer, 256, MPI_CHAR, status.MPI_SOURCE, 0, MPI_COMM_WORLD);
		
	}

	MPI_Finalize();
	return 0;
}