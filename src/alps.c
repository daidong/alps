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

#define UNIQUE_PID(ts, rank, pid)	((ts << 32) + (rank << 16) + pid)  //Every 49 days, it may repeat 
#define BUCKET_NUM 128

int static ALPS_DIVIDE = 2;

int main(int argc, char **argv){
	int rank;
	int total_size;

	MPI_Init(&argc,&argv);
	MPI_Comm_rank(MPI_COMM_WORLD, &rank);
	MPI_Comm_size(MPI_COMM_WORLD, &total_size);
	
	/* create MPI type*/
	const int items = 12;
	int block_length[12] = {1,1,1,1,1,128,128,128,128,128,1,1};
	MPI_Datatype types[12] = {MPI_INT, MPI_LONG_LONG, MPI_LONG_LONG, MPI_LONG_LONG, MPI_LONG_LONG, 
		MPI_CHAR, MPI_CHAR, MPI_CHAR, MPI_CHAR, MPI_CHAR, MPI_INT, MPI_INT};
	MPI_Datatype alps_message_type;
	MPI_Aint offsets[12];
	offsets[0] = offsetof(alps_message, message_header);
	offsets[1] = offsetof(alps_message, pid);
	offsets[2] = offsetof(alps_message, child_pid);
	offsets[3] = offsetof(alps_message, ts1);
	offsets[4] = offsetof(alps_message, ts2);
	offsets[5] = offsetof(alps_message, execname);
	offsets[6] = offsetof(alps_message, argstr);
	offsets[7] = offsetof(alps_message, env);
	offsets[8] = offsetof(alps_message, retstr);
	offsets[9] = offsetof(alps_message, execfile);
	offsets[10] = offsetof(alps_message, fd);
	offsets[11] = offsetof(alps_message, flag);
	MPI_Type_create_struct(items, block_length, offsets, types, &alps_message_type);
	MPI_Type_commit(&alps_message_type);

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
		
		struct alps_exec *all_alive_execs[BUCKET_NUM];
		for (int i = 0; i < BUCKET_NUM; i++) all_alive_execs[i] = NULL;

		while ((read = getline(&line, &len, pipe_fd)) != -1){
			bytes_read += len;
			char *p = (line + 1);

			switch ((*line)){
				case 'a':
					sscanf(p, "%d %lld %d\n", &child_pid, &timestamp, &pid);
					//printf("Create Process - Child-PID: %d, timestamp: %lld, Curr-PID: %d\n", child_pid, timestamp, pid);
					long long unique_pid = UNIQUE_PID(timestamp, rank, pid);
					long long unique_child_pid = UNIQUE_PID(timestamp, rank, child_pid);
					int target_builder = hash(unique_pid, total_size - ALPS_DIVIDE) + ALPS_DIVIDE;
					
					int bucket = unique_child_pid % BUCKET_NUM;
					struct alps_exec *ptr = all_alive_execs[bucket];
					struct alps_exec *pre_ptr = ptr;
					while (ptr != NULL) {
						pre_ptr = ptr;
						ptr = ptr->next;
					}
					ptr = (struct alps_exec *) malloc (sizeof(struct alps_exec));
					pre_ptr->next = ptr;
					ptr->unique_id = unique_child_pid;
					ptr->start_ts = timestamp;
					ptr->pid = child_pid;
					ptr->parent_pid = pid;
					ptr->next = NULL;

					alps_message msg;
					msg.message_header = 97;
					msg.pid = unique_pid;
					msg.child_pid = unique_child_pid;
					msg.ts1 = timestamp;
					MPI_Send(&msg, 1, alps_message_type, target_builder, 0, MPI_COMM_WORLD);

					break;
				case 'b':
					/* process exits. we shall send all information we collected about this process to the builder*/
					sscanf(p, "%d %lld\n", &pid, &timestamp);
					//printf("Exit Process - PID: %d, timestamp: %lld\n", pid, timestamp);
					long long unique_pid = UNIQUE_PID(timestamp, rank, pid);
					int target_builder = hash(unique_pid, total_size - ALPS_DIVIDE) + ALPS_DIVIDE;

					int bucket = unique_pid % BUCKET_NUM;
					struct alps_exec *ptr = all_alive_execs[bucket];
					while (ptr != NULL) {
						if (ptr -> unique_id == unique_pid){
							ptr->end_ts = timestamp;
							//find the content
							alps_message msg;
							msg.message_header = 98;
							msg.pid = unique_pid;
							msg.ts1 = ptr->start_ts;
							msg.ts2 = ptr->end_ts;
							strcpy(msg.execname, ptr->execname);
							strcpy(msg.argstr, ptr->argstr);
							strcpy(msg.env, ptr->env);
							strcpy(msg.retstr, ptr->retstr);
							strcpy(msg.execfile, ptr->execfile);
							MPI_Send(&msg, 1, alps_message_type, target_builder, 0, MPI_COMM_WORLD);
							break;
						}
						ptr = ptr->next;
					}
					if (ptr == NULL){
						//not find this process. Just ignore it
					}

					break;
				case 'c':
					sscanf(p, "%d %lld %s %s %s %s %s\n", &pid, &timestamp, execname, argstr, env, filename, args);
					//printf("Execve Start - PID: %d, timestamp: %lld, execname: %s, argstr: %s, env: %s, filename: %s, args: %s\n", pid, timestamp, execname, argstr, env, filename, args);
					long long unique_pid = UNIQUE_PID(timestamp, rank, pid);
					int bucket = unique_pid % BUCKET_NUM;
					struct alps_exec *ptr = all_alive_execs[bucket];
					while (ptr != NULL) {
						if (ptr -> unique_id == unique_pid){
							strcpy(ptr->execname, execname);
							strcpy(ptr->argstr, argstr);
							strcpy(ptr->env, env);
							strcpy(ptr->execfile, filename);
							break;
						}
						ptr = ptr->next;
					}
					if (ptr == NULL){
						//not find this process. Just ignore it
					}
					break;
				case 'd':
					sscanf(p, "%d %lld %s %s\n", &pid, &timestamp, retstr, env);
					//printf("Execve Return - PID: %d, timestamp: %lld, retstr: %s, env: %s\n", pid, timestamp, retstr, env);
					long long unique_pid = UNIQUE_PID(timestamp, rank, pid);
					int bucket = unique_pid % BUCKET_NUM;
					struct alps_exec *ptr = all_alive_execs[bucket];
					while (ptr != NULL) {
						if (ptr -> unique_id == unique_pid){
							strcpy(ptr->retstr, retstr);
							strcpy(ptr->env, env);
							break;
						}
						ptr = ptr->next;
					}
					if (ptr == NULL){
						//not find this process. Just ignore it
					}

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
		}
		
		fclose(pipe_fd);
	} 
	else {
		
		alps_message recv;
		MPI_Status status;
		MPI_Recv(&recv, 1, alps_message_type, MPI_ANY_SOURCE, 0, MPI_COMM_WORLD, &status);
		MPI_Send(buffer, 256, MPI_CHAR, status.MPI_SOURCE, 0, MPI_COMM_WORLD);
		
	}

	MPI_Finalize();
	return 0;
}