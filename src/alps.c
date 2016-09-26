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

//#define DEBUG_LEVEL_1
//#define DEBUG_LEVEL_2
//#define DEBUG_LEVEL_3 
//#define DEBUG_LEVEL_4

int static ALPS_DIVIDE = 1;

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
		
		const char *fifo_name = "/tmp/test.out"; //this must be a FIFO file
		
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
		
		struct alps_exec all_alive_execs[BUCKET_NUM];
		for (int i = 0; i < BUCKET_NUM; i++) 
			memset(all_alive_execs, 0, sizeof(struct alps_exec));

		int gran = 0;

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
					#ifdef DEBUG_LEVEL_1
						printf("[1] Create Process - Child-PID: %d, timestamp: %lld, Curr-PID: %d\n", child_pid, timestamp, pid);
					#endif
					unique_pid = UNIQUE_PID(timestamp, rank, pid);
					unique_child_pid = UNIQUE_PID(timestamp, rank, child_pid);
					target_builder = hash_long(unique_pid, total_size - ALPS_DIVIDE) + ALPS_DIVIDE;
					
					bucket = child_pid % BUCKET_NUM;
					pre_ptr = &all_alive_execs[bucket];
					ptr = all_alive_execs[bucket].next;
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

					msg.message_header = EVENT_PROCESS_CREAT;
					msg.pid = unique_pid;
					msg.child_pid = unique_child_pid;
					msg.ts1 = timestamp;
					MPI_Send(&msg, 1, alps_message_type, target_builder, 0, MPI_COMM_WORLD);

					break;
				case 'b':
					/* process exits. we shall send all information we collected about this process to the builder */
					sscanf(p, "%d %lld\n", &pid, &timestamp);
					#ifdef DEBUG_LEVEL_1
						printf("[1] Exit Process - PID: %d, timestamp: %lld\n", pid, timestamp);
					#endif
					
					bucket = pid % BUCKET_NUM;
					pre_ptr = &all_alive_execs[bucket];
					ptr = all_alive_execs[bucket].next;
					while (ptr != NULL) {
						if (ptr->pid == pid) break;
						pre_ptr = ptr;
						ptr = ptr->next;
					}
					// if never capture this process, should ignore its I/Os
					if (ptr == NULL) break;

					#ifdef DEBUG_LEVEL_2
						printf("[2] Exit an existing process - PID: %d at %lld\n", pid, timestamp);
					#endif

					unique_pid = ptr->unique_id;
					target_builder = hash_long(unique_pid, total_size - ALPS_DIVIDE) + ALPS_DIVIDE;

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
					#ifdef DEBUG_LEVEL_2
						printf("[2] Exit an existing process; Send Message to %d - PID: %d at %lld\n", 
							target_builder, pid, timestamp);
					#endif
					MPI_Send(&msg, 1, alps_message_type, target_builder, 0, MPI_COMM_WORLD);

					//remove ptr from the list
					pre_ptr->next = ptr->next;
					free(ptr);

					break;
				case 'c':
					sscanf(p, "%d %lld %s %s %s %s %s\n", &pid, &timestamp, execname, argstr, env, filename, args);
					#ifdef DEBUG_LEVEL_1
						printf("[1] Execve Start - PID: %d, timestamp: %lld, execname: %s, argstr: %s, env: %s, filename: %s, args: %s\n", pid, timestamp, execname, argstr, env, filename, args);
					#endif

					bucket = pid % BUCKET_NUM;
					ptr = all_alive_execs[bucket].next;
					while (ptr != NULL) {
						#ifdef DEBUG_LEVEL_3
							printf("iterate existing processes: PID (%d)\n", ptr->pid);
						#endif
						if (ptr->pid == pid) break;
						ptr = ptr->next;
					}
					// if never capture this process, should ignore its I/Os
					if (ptr == NULL) break;

					#ifdef DEBUG_LEVEL_2
						printf("[2] An existing - PID: %d call execve start at %lld\n", pid, timestamp);
					#endif

					unique_pid = ptr->unique_id;

					strcpy(ptr->execname, execname);
					strcpy(ptr->argstr, argstr);
					strcpy(ptr->env, env);
					strcpy(ptr->execfile, filename);
					
					break;
				case 'd':
					sscanf(p, "%d %lld %s %s\n", &pid, &timestamp, retstr, env);
					#ifdef DEBUG_LEVEL_1
						printf("[1] Execve Return - PID: %d, timestamp: %lld, retstr: %s, env: %s\n", pid, timestamp, retstr, env);
					#endif

					bucket = pid % BUCKET_NUM;
					ptr = all_alive_execs[bucket].next;
					while (ptr != NULL) {
						if (ptr->pid == pid) break;
						ptr = ptr->next;
					}
					// if never capture this process, should ignore its I/Os
					if (ptr == NULL) break;

					#ifdef DEBUG_LEVEL_2
						printf("[2] An existing - PID: %d call execve end at %lld\n", pid, timestamp);
					#endif

					unique_pid = ptr->unique_id;

					strcpy(ptr->retstr, retstr);
					strcpy(ptr->env, env);

					break;
				case 'e':
					sscanf(p, "%d %lld %s %d %d\n", &pid, &timestamp, filename, &fd, &flag);
					#ifdef DEBUG_LEVEL_1
						printf("[1] Open - PID: %d, timestamp: %lld, filename: %s, fd: %d, flag:%d\n", pid, timestamp, filename, fd, flag);
					#endif

					bucket = pid % BUCKET_NUM;
					ptr = all_alive_execs[bucket].next;
					while (ptr != NULL) {
						if (ptr->pid == pid) break;
						ptr = ptr->next;
					}
					// if never capture this process, should ignore its I/Os
					if (ptr == NULL) break;

					#ifdef DEBUG_LEVEL_2
						printf("[2] An existing - PID: %d open %s at %lld\n", pid, filename, timestamp);
					#endif

					unique_pid = ptr->unique_id;
					target_builder = hash_str(filename, total_size - ALPS_DIVIDE) + ALPS_DIVIDE;
					if (gran == 0){
						if ((flag & 0x3) == 0) 
							msg.message_header = EVENT_OPEN_RDONLY;
						else
							msg.message_header = EVENT_OPEN_RDWR;

						msg.pid = unique_pid;
						msg.ts1 = timestamp;
						strcpy(msg.execfile, filename);
						#ifdef DEBUG_LEVEL_2
							printf("[2] An existing Open Send Message to %d- PID: %d open %s at %lld\n", 
								target_builder, pid, filename, timestamp);
						#endif
						MPI_Send(&msg, 1, alps_message_type, target_builder, 0, MPI_COMM_WORLD);
					}

					break;
				case 'g':
					sscanf(p, "%d %lld %s %d\n", &pid, &timestamp, filename, &fd);
					#ifdef DEBUG_LEVEL_1
						printf("[1] Close - PID: %d, timestamp: %lld, filename: %s, fd: %d\n", pid, timestamp, filename, fd);
					#endif

					bucket = pid % BUCKET_NUM;
					ptr = all_alive_execs[bucket].next;
					while (ptr != NULL) {
						if (ptr->pid == pid) break;
						ptr = ptr->next;
					}
					// if never capture this process, should ignore its I/Os
					if (ptr == NULL) break;

					#ifdef DEBUG_LEVEL_2
						printf("[2] An existing - PID: %d close %s at %lld\n", pid, filename, timestamp);
					#endif

					unique_pid = ptr->unique_id;
					target_builder = hash_str(filename, total_size - ALPS_DIVIDE) + ALPS_DIVIDE;

					if (gran == 0){
						msg.message_header = EVENT_CLOSE;
						msg.pid = unique_pid;
						strcpy(msg.execfile, filename);
						msg.ts1 = timestamp;
						#ifdef DEBUG_LEVEL_2
							printf("[2] An existing Close Send Message to %d- PID: %d close %s at %lld\n", 
								target_builder, pid, filename, timestamp);
						#endif
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
						
						#ifdef DEBUG_LEVEL_2
							printf("[2] An existing Close Send Message to %d - PID: %d close %s at %lld\n", 
								target_builder, pid, filename, timestamp);
						#endif
						MPI_Send(&msg, 1, alps_message_type, target_builder, 0, MPI_COMM_WORLD);
					}

					break;
				case 'p':
					if (gran == 0) 
						break;
					
					sscanf(p, "%d %lld %s\n", &pid, &timestamp, filename);
					#ifdef DEBUG_LEVEL_1
						printf("[1] Read - PID: %d, timestamp: %lld, filename: %s\n", pid, timestamp, filename);
					#endif

					bucket = pid % BUCKET_NUM;
					ptr = all_alive_execs[bucket].next;
					while (ptr != NULL) {
						if (ptr->pid == pid) break;
						ptr = ptr->next;
					}
					// if never capture this process, should ignore its I/Os
					if (ptr == NULL) break;

					#ifdef DEBUG_LEVEL_2
						printf("[2] An existing - PID: %d read %s at %lld\n", pid, filename, timestamp);
					#endif

					unique_pid = ptr->unique_id;

					target_builder = hash_str(filename, total_size - ALPS_DIVIDE) + ALPS_DIVIDE;

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

					#ifdef DEBUG_LEVEL_2
						printf("[2] An existing Read Send Message to %d- PID: %d read %s at %lld\n", 
							target_builder, pid, filename, timestamp);
					#endif

					MPI_Send(&msg, 1, alps_message_type, target_builder, 0, MPI_COMM_WORLD);
					break;
				case 'q':
					if (gran == 0) 
						break;

					sscanf(p, "%d %lld %s\n", &pid, &timestamp, filename);
					#ifdef DEBUG_LEVEL_1
						printf("[1] Write - PID: %d, timestamp: %lld, filename: %s\n", pid, timestamp, filename);
					#endif

					bucket = pid % BUCKET_NUM;
					ptr = all_alive_execs[bucket].next;
					while (ptr != NULL) {
						if (ptr->pid == pid) break;
						ptr = ptr->next;
					}
					// if never capture this process, should ignore its I/Os
					if (ptr == NULL) break;

					#ifdef DEBUG_LEVEL_2
						printf("[2] An existing - PID: %d write %s at %lld\n", pid, filename, timestamp);
					#endif

					unique_pid = ptr->unique_id;
					target_builder = hash_str(filename, total_size - ALPS_DIVIDE) + ALPS_DIVIDE;
					

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

					#ifdef DEBUG_LEVEL_2
						printf("[2] An existing Write Send Message to %d - PID: %d write %s at %lld\n", 
							target_builder, pid, filename, timestamp);
					#endif
					MPI_Send(&msg, 1, alps_message_type, target_builder, 0, MPI_COMM_WORLD);
					break;
			}
		}
		
		fclose(pipe_fd);
	} 
	else {
		char db_file[32], db_env[32];
		sprintf(db_file, "%s%d.file", "/tmp/db", rank);
		sprintf(db_env, "%s%d.env", "/tmp/db", rank);
		init_db(db_file, db_env);

		while (1){
			alps_message recv;
			MPI_Status status;
			MPI_Recv(&recv, 1, alps_message_type, MPI_ANY_SOURCE, 0, MPI_COMM_WORLD, &status);
			//MPI_Send(buffer, 256, MPI_CHAR, status.MPI_SOURCE, 0, MPI_COMM_WORLD);

			long long unique_pid, unique_child_pid;
		
			switch (recv.message_header) {
				case EVENT_PROCESS_CREAT:
					unique_pid = recv.pid;
					unique_child_pid = recv.child_pid;
					#ifdef DEBUG_LEVEL_4
						printf("[BUILDER] Process %lld create Child %lld at %lld\n", 
							unique_pid, unique_child_pid, recv.ts1);
					#endif
					insert((char *) &unique_pid, sizeof(long long), (char *) &unique_child_pid, sizeof(long long),
						EDGE_PARENT_CHILD, recv.ts1, (char *) &pid, sizeof(long long));

					break;
				case EVENT_PROCESS_EXIT:
					unique_pid = recv.pid;
					#ifdef DEBUG_LEVEL_4
						printf("[BUILDER] Process %lld Start at %lld. Exit at %lld with 
							Metadata: execname (%s), argstr(%s), env(%s), retstr(%s), execfile(%s)\n", 
							unique_pid, recv.ts1, recv.ts2, 
							recv.execname, recv.argstr, recv.env, recv.retstr, recv.execfile);
					#endif
					insert((char *) &unique_pid, sizeof(long long), ATTR_EXEC_NAME, 16, 
						EDGE_ATTR, recv.ts1, recv.execname, strlen(recv.execname));

					insert((char *) &unique_pid, sizeof(long long), ATTR_ARG_STR, 16, 
						EDGE_ATTR, recv.ts1, recv.argstr, strlen(recv.argstr));
					
					insert((char *) &unique_pid, sizeof(long long), ATTR_ENV_STR, 16, 
						EDGE_ATTR, recv.ts1, recv.env, strlen(recv.env));
					
					insert((char *) &unique_pid, sizeof(long long), ATTR_RET_STR, 16, 
						EDGE_ATTR, recv.ts1, recv.retstr, strlen(recv.retstr));

					insert((char *) &unique_pid, sizeof(long long), ATTR_EXEC_FILE, 16, 
						EDGE_ATTR, recv.ts1, recv.execfile, strlen(recv.execfile));

					insert((char *) &unique_pid, sizeof(long long), ATTR_START_TS, 16, 
						EDGE_ATTR, recv.ts1, (char *) &recv.ts1, sizeof(long long));
					insert((char *) &unique_pid, sizeof(long long), ATTR_END_TS, 16, 
						EDGE_ATTR, recv.ts1, (char *) &recv.ts2, sizeof(long long));
					
					break;
					
				case EVENT_OPEN_RDONLY:
					unique_pid = recv.pid;
					#ifdef DEBUG_LEVEL_4
						printf("[BUILDER Process %lld read Open file %s at %lld\n", unique_pid, recv.execfile, recv.ts1);
					#endif
					break;
				case EVENT_OPEN_RDWR:
					unique_pid = recv.pid;
					#ifdef DEBUG_LEVEL_4
						printf("[BUILDER Process %lld write Open file %s at %lld\n", unique_pid, recv.execfile, recv.ts1);
					#endif
					break;
				case EVENT_CLOSE:
					unique_pid = recv.pid;
					#ifdef DEBUG_LEVEL_4
						printf("[BUILDER Process %lld Close file %s at %lld\n", unique_pid, recv.execfile, recv.ts1);
					#endif
					break;						
				case EVENT_READ:
					unique_pid = recv.pid;
					#ifdef DEBUG_LEVEL_4
						printf("[BUILDER Process %lld Read file %s at %lld\n", unique_pid, recv.execfile, recv.ts1);
					#endif
					break;
				case EVENT_WRITE:
					unique_pid = recv.pid;
					#ifdef DEBUG_LEVEL_4
						printf("[BUILDER Process %lld Write file %s at %lld\n", unique_pid, recv.execfile, recv.ts1);
					#endif
					break;
				case EVENT_FIRST_READ:
					unique_pid = recv.pid;
					#ifdef DEBUG_LEVEL_4
						printf("[BUILDER Process %lld First Read file %s at %lld\n", unique_pid, recv.execfile, recv.ts1);
					#endif
					break;
				case EVENT_FIRST_WRITE:
					unique_pid = recv.pid;
					#ifdef DEBUG_LEVEL_4
						printf("[BUILDER Process %lld First Write file %s at %lld\n", unique_pid, recv.execfile, recv.ts1);
					#endif
					break;
				case EVENT_LAST_RW:
					unique_pid = recv.pid;
					#ifdef DEBUG_LEVEL_4
						printf("[BUILDER Process %lld Last Read file %s at %lld; Last Write File %s at %lld\n", unique_pid, recv.execfile, recv.ts1, recv.retstr, recv.ts2);
					#endif
					break;
			}
		}
	}

	MPI_Finalize();
	return 0;
}