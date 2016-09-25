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

#include "alps.h"

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
		for (int i = 0; i < BUCKET_NUM; i++) 
			all_alive_execs[i] = NULL;

		int gran = 2;

		while ((read = getline(&line, &len, pipe_fd)) != -1){
			bytes_read += len;
			char *p = (line + 1);

			long long unique_pid, unique_child_pid;
			int target_builder, bucket;
			struct alps_exec *ptr, *pre_ptr;
			alps_message msg;

			switch ((*line)){
				case 'a':
					sscanf(p, "%d %lld %d\n", &child_pid, &timestamp, &pid);
					//printf("Create Process - Child-PID: %d, timestamp: %lld, Curr-PID: %d\n", child_pid, timestamp, pid);
					unique_pid = UNIQUE_PID(timestamp, rank, pid);
					unique_child_pid = UNIQUE_PID(timestamp, rank, child_pid);
					target_builder = hash_long(unique_pid, total_size - ALPS_DIVIDE) + ALPS_DIVIDE;
					
					bucket = unique_child_pid % BUCKET_NUM;
					ptr = all_alive_execs[bucket];
					pre_ptr = ptr;
					while (ptr != NULL) {
						pre_ptr = ptr;
						ptr = ptr->next;
					}
					ptr = (struct alps_exec *) malloc (sizeof(struct alps_exec));
					if (pre_ptr != NULL)
						pre_ptr->next = ptr;
					ptr->unique_id = unique_child_pid;
					ptr->start_ts = timestamp;
					ptr->pid = child_pid;
					ptr->parent_pid = pid;
					ptr->next = NULL;

					msg.message_header = EVENT_PROCESS_CREAT;
					msg.pid = unique_pid;
					msg.child_pid = unique_child_pid;
					msg.ts1 = timestamp;
					MPI_Send(&msg, 1, alps_message_type, target_builder, 0, MPI_COMM_WORLD);

					break;
				case 'b':
					/* process exits. we shall send all information we collected about this process to the builder*/
					sscanf(p, "%d %lld\n", &pid, &timestamp);
					//printf("Exit Process - PID: %d, timestamp: %lld\n", pid, timestamp);
					unique_pid = UNIQUE_PID(timestamp, rank, pid);
					target_builder = hash_long(unique_pid, total_size - ALPS_DIVIDE) + ALPS_DIVIDE;

					bucket = unique_pid % BUCKET_NUM;
					ptr = all_alive_execs[bucket];
					while (ptr != NULL) {
						if (ptr -> unique_id == unique_pid){
							ptr->end_ts = timestamp;
							//find the content
							msg.message_header = EVENT_PROCESS_EXIT;
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
					unique_pid = UNIQUE_PID(timestamp, rank, pid);
					bucket = unique_pid % BUCKET_NUM;
					ptr = all_alive_execs[bucket];
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
					unique_pid = UNIQUE_PID(timestamp, rank, pid);
					bucket = unique_pid % BUCKET_NUM;
					ptr = all_alive_execs[bucket];
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
					//printf("Open - PID: %d, timestamp: %lld, filename: %s, fd: %d, flag:%d\n", pid, timestamp, filename, fd, flag);
					unique_pid = UNIQUE_PID(timestamp, rank, pid);
					target_builder = hash_str(filename, total_size - ALPS_DIVIDE) + ALPS_DIVIDE;
					
					bucket = unique_pid % BUCKET_NUM;
					ptr = all_alive_execs[bucket];
					while (ptr != NULL) {
						if (ptr->unique_id == unique_pid) break;
						ptr = ptr->next;
					}
					// if never capture this process, should ignore its I/Os
					if (ptr == NULL) break;

					if (gran == 0){
						//101: OPEN_READONLY; 102: OPEN_READWRITE
						if ((flag & 0x3) == 0) 
							msg.message_header = EVENT_OPEN_RDONLY;
						else
							msg.message_header = EVENT_OPEN_RDWR;

						msg.pid = unique_pid;
						msg.ts1 = timestamp;
						strcpy(msg.execfile, filename);
						MPI_Send(&msg, 1, alps_message_type, target_builder, 0, MPI_COMM_WORLD);
					}

					break;
				case 'g':
					sscanf(p, "%d %lld %s %d\n", &pid, &timestamp, filename, &fd);
					//printf("Close - PID: %d, timestamp: %lld, filename: %s, fd: %d\n", pid, timestamp, filename, fd);
					unique_pid = UNIQUE_PID(timestamp, rank, pid);
					target_builder = hash_str(filename, total_size - ALPS_DIVIDE) + ALPS_DIVIDE;

					bucket = unique_pid % BUCKET_NUM;
					ptr = all_alive_execs[bucket];
					while (ptr != NULL) {
						if (ptr->unique_id == unique_pid) break;
						ptr = ptr->next;
					}
					// if never capture this process, should ignore its I/Os
					if (ptr == NULL) break;

					if (gran == 0){
						msg.message_header = EVENT_CLOSE;
						msg.pid = unique_pid;
						strcpy(msg.execfile, filename);
						msg.ts1 = timestamp;
						MPI_Send(&msg, 1, alps_message_type, target_builder, 0, MPI_COMM_WORLD);

					} else if (gran == 1){
						/* If no LAST_READ or LAST_WRITE, we should send out the CLOSE event to guarantee to end the events*/
						// @TODO: not sure about previous comment. check it later.
						msg.message_header = EVENT_LAST_RW;
						msg.pid = unique_pid;
						
						if (ptr->last_read[0] != '\0'){
							msg.ts1 = ptr->last_read_ts;
							strcpy(msg.execfile, ptr->last_read);
						} else {
							msg.ts1 = timestamp;
							strcpy(msg.execfile, filename);
						}

						if (ptr->last_write[0] != '\0'){
							msg.ts2 = ptr->last_write_ts;
							strcpy(msg.retstr, ptr->last_write);
						} else {
							msg.ts2 = timestamp;
							strcpy(msg.retstr, filename);
						}
						
						MPI_Send(&msg, 1, alps_message_type, target_builder, 0, MPI_COMM_WORLD);
					}

					break;
				case 'p':
					sscanf(p, "%d %lld %s\n", &pid, &timestamp, filename);
					//printf("Read - PID: %d, timestamp: %lld, filename: %s\n", pid, timestamp, filename);
					unique_pid = UNIQUE_PID(timestamp, rank, pid);
					target_builder = hash_str(filename, total_size - ALPS_DIVIDE) + ALPS_DIVIDE;
					
					bucket = unique_pid % BUCKET_NUM;
					ptr = all_alive_execs[bucket];
					while (ptr != NULL) {
						if (ptr->unique_id == unique_pid) break;
						ptr = ptr->next;
					}
					if (ptr == NULL) break;

					strcpy(ptr->last_read, filename);
					ptr->last_read_ts = timestamp;

					if (ptr->first_read[0] == '\0'){
						strcpy(ptr->first_read, filename);
						ptr->first_read_ts = timestamp;
					}

					msg.pid = unique_pid;
					msg.ts1 = timestamp;
					strcpy(msg.execfile, filename);
							
					if (gran == 1){ //	First/Last
						msg.message_header = EVENT_FIRST_READ;						
					} else if (gran == 2){ // Operation
						msg.message_header = EVENT_READ;
					}

					MPI_Send(&msg, 1, alps_message_type, target_builder, 0, MPI_COMM_WORLD);
					break;
				case 'q':
					sscanf(p, "%d %lld %s\n", &pid, &timestamp, filename);
					//printf("Write - PID: %d, timestamp: %lld, filename: %s\n", pid, timestamp, filename);
					unique_pid = UNIQUE_PID(timestamp, rank, pid);
					target_builder = hash_str(filename, total_size - ALPS_DIVIDE) + ALPS_DIVIDE;
					
					bucket = unique_pid % BUCKET_NUM;
					ptr = all_alive_execs[bucket];
					while (ptr != NULL) {
						if (ptr->unique_id == unique_pid) break;
						ptr = ptr->next;
					}
					if (ptr == NULL) break;

					strcpy(ptr->last_write, filename);
					ptr->last_write_ts = timestamp;

					if (ptr->first_write[0] == '\0'){
						strcpy(ptr->first_write, filename);
						ptr->first_write_ts = timestamp;
					}

					msg.pid = unique_pid;
					msg.ts1 = timestamp;
					strcpy(msg.execfile, filename);
							
					if (gran == 1){ //	First/Last
						msg.message_header = EVENT_FIRST_WRITE;						
					} else if (gran == 2){ // Operation
						msg.message_header = EVENT_WRITE;
					}

					MPI_Send(&msg, 1, alps_message_type, target_builder, 0, MPI_COMM_WORLD);
					break;
			}
		}
		
		fclose(pipe_fd);
	} 
	else {
		alps_message recv;
		MPI_Status status;
		MPI_Recv(&recv, 1, alps_message_type, MPI_ANY_SOURCE, 0, MPI_COMM_WORLD, &status);
		//MPI_Send(buffer, 256, MPI_CHAR, status.MPI_SOURCE, 0, MPI_COMM_WORLD);

		long long unique_pid, unique_child_pid;

		switch (recv.message_header) {
			case EVENT_PROCESS_CREAT:
				unique_pid = recv.pid;
				unique_child_pid = recv.child_pid;
				printf("[BUILDER] Process %lld create Child %lld at %lld\n", unique_pid, unique_child_pid, recv.ts1);
				break;
			case EVENT_PROCESS_EXIT:
				unique_pid = recv.pid;
				printf("[BUILDER] Process %lld Start at %lld. Exit at %lld with Metadata: execname (%s), argstr(%s), env(%s), retstr(%s), execfile(%s)\n", unique_pid, recv.ts1, recv.ts2, recv.execname, recv.argstr, recv.env, recv.retstr, recv.execfile);
				break;
			case EVENT_OPEN_RDONLY:
				unique_pid = recv.pid;
				printf("[BUILDER Process %lld read Open file %s at %lld\n", unique_pid, recv.execfile, recv.ts1);
				break;
			case EVENT_OPEN_RDWR:
				unique_pid = recv.pid;
				printf("[BUILDER Process %lld write Open file %s at %lld\n", unique_pid, recv.execfile, recv.ts1);
				break;
			case EVENT_CLOSE:
				unique_pid = recv.pid;
				printf("[BUILDER Process %lld Close file %s at %lld\n", unique_pid, recv.execfile, recv.ts1);
				break;						
			case EVENT_READ:
				unique_pid = recv.pid;
				printf("[BUILDER Process %lld Read file %s at %lld\n", unique_pid, recv.execfile, recv.ts1);
				break;
			case EVENT_WRITE:
				unique_pid = recv.pid;
				printf("[BUILDER Process %lld Write file %s at %lld\n", unique_pid, recv.execfile, recv.ts1);
				break;
			case EVENT_FIRST_READ:
				unique_pid = recv.pid;
				printf("[BUILDER Process %lld First Read file %s at %lld\n", unique_pid, recv.execfile, recv.ts1);
				break;
			case EVENT_FIRST_WRITE:
				unique_pid = recv.pid;
				printf("[BUILDER Process %lld First Write file %s at %lld\n", unique_pid, recv.execfile, recv.ts1);
				break;
			case EVENT_LAST_RW:
				unique_pid = recv.pid;
				printf("[BUILDER Process %lld Last Read file %s at %lld; Last Write File %s at %lld\n", 
					unique_pid, recv.execfile, recv.ts1, recv.retstr, recv.ts2);
				break;
		}
	}

	MPI_Finalize();
	return 0;
}