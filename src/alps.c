#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <limits.h>
#include <string.h>

#include <mpi.h>

#include "util.h"
#include "store.h"
#include "dbkey.h"

int static ALPS_DIVIDE = 2;

int main(int argc, char **argv){
  int rank;
  
  MPI_Init(&argc,&argv);
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);

	/*
  char hostname[256];
	gethostname(hostname,255);
	*/
	
	if (rank < ALPS_DIVIDE){
		
		const char *fifo_name = "/tmp/my_fifo"; //this must be a local file
		
		FILE *pipe_fd;
		size_t len = 0;
		ssize_t read;
		
		char *line = NULL;
		int bytes_read = 0;
		
		pipe_fd = fopen(fifo_name, "r");
		if (pipe_fd == NULL)
			exit(0);
		
		while ((read = getline(&line, &len, pipe_fd)) != -1){
			bytes_read += len;

			switch ((*line)){
				case 'a':

					break;
				case 'b':
					break;
				case 'c':
					break;
				case 'd':
					break;
				case 'e':
					break;
				case 'g':
					break;
				case 'p':
					break;
				case 'q':
					break;
				default:

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