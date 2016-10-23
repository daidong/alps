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
#define GET_PID(upid) (upid & 0xFFFF)

static unsigned long long UNIQUE_FID(char *file, int rank) {
	if (strstr(file, "/mnt/orangefs") == file || strstr(file, "/proj") == file)
		return hash_file(file);
	else {
		char new_file[256];
		sprintf(new_file, "%s:%d", file, rank);
		return hash_file(new_file);
	}
}

//			|| strstr(file, "/tmp/") == file
static int filter_pipe_file(char *file) {
	if (strstr(file, "pipe:") == file
			|| strstr(file, "UNDEFINED") == file
			|| strstr(file, "anon_inode") == file
			|| strstr(file, "socket:") == file
			|| strstr(file, "/proc/") == file
			|| strstr(file, "/dev/") == file
			|| strstr(file, "/usr/") == file
			|| strstr(file, "/etc/") == file
			|| strstr(file, "/run/") == file)
		return 0;
	else
		return 1;
}

static void builder_process_create_handler(alps_message recv) {
	unsigned long long unique_pid = recv.pid;
	unsigned long long unique_child_pid = recv.child_pid;

	char parent_child[32], child_parent[32];
	sprintf(parent_child, "Parent To Child: %lld", recv.ts1);
	sprintf(child_parent, "Child To Parent: %lld", recv.ts1);

	insert((char *) &unique_pid, sizeof(unsigned long long),
			(char *) &unique_child_pid, sizeof(unsigned long long),
			EDGE_PARENT_CHILD, 0, (char *) parent_child,
			strlen(parent_child));

	insert((char *) &unique_child_pid, sizeof(unsigned long long),
			(char *) &unique_pid, sizeof(unsigned long long), EDGE_CHILD_PARENT,
			0, (char *) child_parent, strlen(child_parent));
}

static void builder_process_exit_handler(alps_message recv) {
	unsigned long long unique_pid = recv.pid;

	insert((char *) &unique_pid, sizeof(unsigned long long),
			(char *) &ATTR_EXEC_NAME, sizeof(unsigned long long), EDGE_ATTR,
			0, recv.execname, strlen(recv.execname));

	insert((char *) &unique_pid, sizeof(unsigned long long),
			(char *) &ATTR_ARG_STR, sizeof(unsigned long long), EDGE_ATTR,
			0, recv.argstr, strlen(recv.argstr));

	insert((char *) &unique_pid, sizeof(unsigned long long),
			(char *) &ATTR_ENV_STR, sizeof(unsigned long long), EDGE_ATTR,
			0, recv.env, strlen(recv.env));

	insert((char *) &unique_pid, sizeof(unsigned long long),
			(char *) &ATTR_RET_STR, sizeof(unsigned long long), EDGE_ATTR,
			0, recv.retstr, strlen(recv.retstr));

	insert((char *) &unique_pid, sizeof(unsigned long long),
			(char *) &ATTR_EXEC_FILE, sizeof(unsigned long long), EDGE_ATTR,
			0, recv.execfile, strlen(recv.execfile));

	char sts1[32], sts2[32];
	sprintf(sts1, "%lld", recv.ts1);
	sprintf(sts2, "%lld", recv.ts2);

	insert((char *) &unique_pid, sizeof(unsigned long long),
			(char *) &ATTR_START_TS, sizeof(unsigned long long), EDGE_ATTR,
			0, sts1, strlen(sts1));
	insert((char *) &unique_pid, sizeof(unsigned long long),
			(char *) &ATTR_END_TS, sizeof(unsigned long long), EDGE_ATTR,
			0, sts2, strlen(sts2));
}

static void builder_file_event_handler(int event, alps_message recv, int rank,
		struct alps_version all_accessed_files[BUCKET_NUM]) {

	unsigned long long unique_pid = recv.pid;
	unsigned long long unique_fid = UNIQUE_FID(recv.execfile, rank);
	unsigned long bucket = (unsigned long) (unique_fid % BUCKET_NUM);
	struct alps_version *pre_ptr = &all_accessed_files[bucket];
	struct alps_version *ptr = all_accessed_files[bucket].hash_next;

	while (ptr != NULL) {
		if (ptr->unique_fid == unique_fid)
			break;
		pre_ptr = ptr;
		ptr = ptr->hash_next;
	}

	//first time access this file. create head of the hash list;
	if (ptr == NULL) {
		ptr = (struct alps_version *) malloc(sizeof(struct alps_version));
		ptr->unique_fid = unique_fid;
		ptr->timestamp = 0;
		ptr->version = 0;
		ptr->event = 0;
		ptr->hash_next = NULL;
		ptr->version_next = NULL;
		pre_ptr->hash_next = ptr;
	}

	struct alps_version *internal_pre_ptr = ptr;
	struct alps_version *internal_ptr = ptr->version_next;

#ifdef DEBUG_BUILDER_LEVEL_1
	printf("[4.5] [BUILDER] File Event: %d\n", event);
#endif

	if (event == EVENT_FIRST_WRITE || event == EVENT_OPEN_RDWR
			|| event == EVENT_OPEN_WRONLY) {
		while (internal_ptr != NULL) {
			if (internal_ptr->timestamp > recv.ts1) //recv.ts1 - c
				break;
			internal_pre_ptr = internal_ptr;
			internal_ptr = internal_ptr->version_next;
		}

		internal_ptr = (struct alps_version *) malloc(
				sizeof(struct alps_version));
		internal_ptr->unique_fid = unique_fid;
		internal_ptr->unique_pid = unique_pid;
		internal_ptr->timestamp = recv.ts1;
		internal_ptr->event = event;
		internal_ptr->hash_next = NULL;
		internal_ptr->version_next = NULL;
		internal_ptr->version = (internal_pre_ptr->version + 1);

		// insert the new element;
		internal_ptr->version_next = internal_pre_ptr->version_next;
		internal_pre_ptr->version_next = internal_ptr;
#ifdef DEBUG_BUILDER_LEVEL_1
		printf("[4.5] [BUILDER] File Event: %d, Insert Write Behaviors\n",
				event);
#endif
		/*
		 * this is a write edge between PID to the file, with correct version.
		 */
		insert((char *) &unique_pid, sizeof(unsigned long long),
				(char *) &unique_fid, sizeof(unsigned long long), EDGE_WRITE,
				(long long) internal_ptr->version, recv.execfile,
				strlen(recv.execfile));
		insert((char *) &unique_fid, sizeof(unsigned long long),
				(char *) &unique_pid, sizeof(unsigned long long), EDGE_WRITE_BY,
				(long long) internal_ptr->version, recv.execfile,
				strlen(recv.execfile));
	}

	if (event == EVENT_LAST_READ || event == EVENT_CLOSE_RDONLY
			|| event == EVENT_CLOSE_RDWR) {
		while (internal_ptr != NULL) {
			if (internal_ptr->timestamp > recv.ts1) //recv.ts1 - c
				break;
			internal_pre_ptr = internal_ptr;
			internal_ptr = internal_ptr->version_next;
		}
#ifdef DEBUG_BUILDER_LEVEL_1
		printf("[4.5] [BUILDER] File Event: %d, Get Read Version\n", event);
#endif
		/*
		 * this is a read edge between PID to the file.
		 */
		insert((char *) &unique_pid, sizeof(unsigned long long),
				(char *) &unique_fid, sizeof(unsigned long long), EDGE_READ,
				(long long) internal_pre_ptr->version, recv.execfile,
				strlen(recv.execfile));
		insert((char *) &unique_fid, sizeof(unsigned long long),
				(char *) &unique_pid, sizeof(unsigned long long), EDGE_READ_BY,
				(long long) internal_pre_ptr->version, recv.execfile,
				strlen(recv.execfile));

		//Scan from the begining again, delete corresponding READ behaviors
		internal_pre_ptr = ptr;
		internal_ptr = ptr->version_next;
		while (internal_ptr != NULL) {
			if (internal_ptr->timestamp > recv.ts1
					&& internal_ptr->unique_pid == unique_pid) {

				// Break once meet the first READ behavior
				if (internal_ptr->event == EVENT_FIRST_READ
						|| internal_ptr->event == EVENT_OPEN_RDONLY
						|| internal_ptr->event == EVENT_OPEN_RDWR) {
					struct alps_version *free_ptr = internal_ptr;
					internal_pre_ptr->version_next = internal_ptr->version_next;
					internal_ptr = internal_ptr->version_next;
					free(free_ptr);
					break;
				}
			}
			internal_pre_ptr = internal_ptr;
			if (internal_ptr != NULL)
				internal_ptr = internal_ptr->version_next;
		}
	}

	if (event == EVENT_FIRST_READ || event == EVENT_OPEN_RDONLY
			|| event == EVENT_OPEN_RDWR) {
		while (internal_ptr != NULL) {
			if (internal_ptr->timestamp > recv.ts1) //recv.ts1 - c
				break;
			internal_pre_ptr = internal_ptr;
			internal_ptr = internal_ptr->version_next;
		}
		//insert a new FIRST_READ or OPEN_RDONLY entry
		internal_ptr = (struct alps_version *) malloc(
				sizeof(struct alps_version));
		internal_ptr->unique_fid = unique_fid;
		internal_ptr->unique_pid = unique_pid;
		internal_ptr->timestamp = recv.ts1;
		internal_ptr->event = event;
		internal_ptr->hash_next = NULL;
		internal_ptr->version_next = NULL;
		internal_ptr->version = internal_pre_ptr->version;

		// insert the new element;
		internal_ptr->version_next = internal_pre_ptr->version_next;
		internal_pre_ptr->version_next = internal_ptr;
#ifdef DEBUG_BUILDER_LEVEL_1
		printf(
				"[4.5] [BUILDER] File Event: %d, Insert Start of Read Behavior\n",
				event);
#endif
		//Scan from the begining again, delete all WRITE behaviors before the first READ behavior
		//note that, we include the tolerable delay due to network (1s in this case).
		internal_pre_ptr = ptr;
		internal_ptr = ptr->version_next;
		while (internal_ptr != NULL) {
			if (internal_ptr->timestamp > (recv.ts1 - TOLERABLE_DELAY)) {
				// Break once meet the first READ behavior
				if (internal_ptr->event == EVENT_FIRST_READ
						|| internal_ptr->event == EVENT_OPEN_RDONLY
						|| internal_ptr->event == EVENT_OPEN_RDWR)
					break;

				if (internal_ptr->event == EVENT_FIRST_WRITE
						|| internal_ptr->event == EVENT_OPEN_WRONLY) {
					struct alps_version *free_ptr = internal_ptr;
					internal_pre_ptr->version_next = internal_ptr->version_next;
					internal_ptr = internal_ptr->version_next;
					free(free_ptr);
				}
			}
			internal_pre_ptr = internal_ptr;
			if (internal_ptr != NULL)
				internal_ptr = internal_ptr->version_next;
		}
	}

	if (event == EVENT_LAST_WRITE || event == EVENT_CLOSE_WRONLY) {

	}
}

int main(int argc, char **argv) {
	int rank;
	int total_size;

	MPI_Init(&argc, &argv);
	MPI_Comm_rank(MPI_COMM_WORLD, &rank);
	MPI_Comm_size(MPI_COMM_WORLD, &total_size);

	/* create MPI type*/
	const int items = 12;
	int block_length[12] = { 1, 1, 1, 1, 1,
	STR_MAX_LEN, STR_MAX_LEN, STR_MAX_LEN, STR_MAX_LEN, STR_MAX_LEN, 1, 1 };
	MPI_Datatype types[12] = { MPI_INT, MPI_LONG_LONG, MPI_LONG_LONG,
	MPI_LONG_LONG, MPI_LONG_LONG,
	MPI_CHAR, MPI_CHAR, MPI_CHAR, MPI_CHAR, MPI_CHAR, MPI_INT, MPI_INT };
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
	MPI_Type_create_struct(items, block_length, offsets, types,
			&alps_message_type);
	MPI_Type_commit(&alps_message_type);

	if (rank < ALPS_DIVIDE) {

		const char *fifo_name = "/tmp/ramdisk/test.out"; //this must be a FIFO file

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
		char execname[STR_MAX_LEN], argstr[STR_MAX_LEN], args[STR_MAX_LEN];
		char env[STR_MAX_LEN], filename[STR_MAX_LEN], retstr[STR_MAX_LEN];

		struct alps_exec all_alive_execs[BUCKET_NUM];
		for (int i = 0; i < BUCKET_NUM; i++)
			memset(&all_alive_execs[i], 0, sizeof(struct alps_exec));

		while ((read = getline(&line, &len, pipe_fd)) != -1) {
			bytes_read += len;
			char *p = (line + 1);

			unsigned long long unique_pid, unique_child_pid;
			unsigned long long unique_fid;
			unsigned long target_builder, bucket;
			struct alps_exec *ptr, *pre_ptr;
			struct file_access *f_pre_ptr, *f_ptr;
			alps_message msg;
			memset(&msg, 0, sizeof(alps_message));
			long long fr_ts = 0, lr_ts = 0, fw_ts = 0, lw_ts = 0;

			//printf("AGGREGATOR GETS LINE: %s Start\n", line);

			switch ((*line)) {
			case EVENT_PROCESS_CREAT:
				sscanf(p, "%d %lld %d\n", &child_pid, &timestamp, &pid);
#ifdef DEBUG_AGG_LEVEL_1
				printf("[1] Create Process - Child-PID: %d, timestamp: %lld, Curr-PID: %d\n", child_pid, timestamp, pid);
#endif
				unique_pid = UNIQUE_PID(timestamp, rank, pid);
				unique_child_pid = UNIQUE_PID(timestamp, rank, child_pid);
				target_builder = hash_long(unique_pid,
						total_size - ALPS_DIVIDE) + ALPS_DIVIDE;

				bucket = child_pid % BUCKET_NUM;
				pre_ptr = &all_alive_execs[bucket];
				ptr = all_alive_execs[bucket].next;
				while (ptr != NULL) {
					pre_ptr = ptr;
					ptr = ptr->next;
				}
				ptr = (struct alps_exec *) malloc(sizeof(struct alps_exec));

				pre_ptr->next = ptr;
				ptr->unique_id = unique_child_pid;
				ptr->start_ts = timestamp;
				ptr->end_ts = timestamp;
				ptr->pid = child_pid;
				ptr->parent_pid = pid;
				for (int i = 0; i < BUCKET_NUM; i++)
					memset(&(ptr->all_opened_files[i]), 0,
							sizeof(struct file_access));

				ptr->next = NULL;

				msg.message_header = EVENT_PROCESS_CREAT;
				msg.pid = unique_pid;
				msg.child_pid = unique_child_pid;
				msg.ts1 = timestamp;
				MPI_Send(&msg, 1, alps_message_type, target_builder, 0,
				MPI_COMM_WORLD);

				break;

			case EVENT_PROCESS_EXIT:
				sscanf(p, "%d %lld\n", &pid, &timestamp);
#ifdef DEBUG_AGG_LEVEL_1
				printf("[1] Exit Process - PID: %d, timestamp: %lld\n", pid, timestamp);
#endif

				bucket = pid % BUCKET_NUM;
				pre_ptr = &all_alive_execs[bucket];
				ptr = all_alive_execs[bucket].next;
				while (ptr != NULL) {
					if (ptr->pid == pid)
						break;
					pre_ptr = ptr;
					ptr = ptr->next;
				}
				if (ptr == NULL)
					break;

#ifdef DEBUG_AGG_LEVEL_2
				printf("[2] Exit an existing process - PID: %d at %lld\n", pid, timestamp);
#endif

				unique_pid = ptr->unique_id;
				target_builder = hash_long(unique_pid,
						total_size - ALPS_DIVIDE) + ALPS_DIVIDE;

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

				MPI_Send(&msg, 1, alps_message_type, target_builder, 0,
				MPI_COMM_WORLD);

				//remove ptr from the list
				pre_ptr->next = ptr->next;
				//@TODO: before freeing ptr, we should free all its accessed files first.
				free(ptr);

				break;
			case EVENT_EXECVE_START:
				sscanf(p, "%d %lld %s %s %s %s %s\n", &pid, &timestamp,
						execname, argstr, env, filename, args);
#ifdef DEBUG_AGG_LEVEL_1
				printf("[1] Execve Start - PID: %d, timestamp: %lld, execname: %s, argstr: %s, env: %s, filename: %s, args: %s\n", pid, timestamp, execname, argstr, env, filename, args);
#endif

				bucket = pid % BUCKET_NUM;
				ptr = all_alive_execs[bucket].next;
				while (ptr != NULL) {
					if (ptr->pid == pid)
						break;
					ptr = ptr->next;
				}
				if (ptr == NULL)
					break;

#ifdef DEBUG_AGG_LEVEL_2
				printf("[2] An existing - PID: %d call execve start at %lld\n", pid, timestamp);
#endif

				unique_pid = ptr->unique_id;

				strcpy(ptr->execname, execname);
				strcpy(ptr->argstr, argstr);
				strcpy(ptr->env, env);
				strcpy(ptr->execfile, filename);

				break;

			case EVENT_EXECVE_END:
				sscanf(p, "%d %lld %s %s\n", &pid, &timestamp, retstr, env);
#ifdef DEBUG_AGG_LEVEL_1
				printf("[1] Execve Return - PID: %d, timestamp: %lld, retstr: %s, env: %s\n", pid, timestamp, retstr, env);
#endif

				bucket = pid % BUCKET_NUM;
				ptr = all_alive_execs[bucket].next;
				while (ptr != NULL) {
					if (ptr->pid == pid)
						break;
					ptr = ptr->next;
				}
				if (ptr == NULL)
					break;

#ifdef DEBUG_AGG_LEVEL_2
				printf("[2] An existing - PID: %d call execve end at %lld\n", pid, timestamp);
#endif

				unique_pid = ptr->unique_id;

				strcpy(ptr->retstr, retstr);
				strcpy(ptr->env, env);

				break;

			case EVENT_OPEN:
				sscanf(p, "%d %lld %s %d %d\n", &pid, &timestamp, filename, &fd,
						&flag);
#ifdef DEBUG_AGG_LEVEL_1
				printf("[1] Open - PID: %d, timestamp: %lld, filename: %s, fd: %d, flag:%d\n", pid, timestamp, filename, fd, flag);
#endif
				if (filter_pipe_file(filename) == 0)
					break;

				bucket = pid % BUCKET_NUM;
				ptr = all_alive_execs[bucket].next;
				while (ptr != NULL) {
					if (ptr->pid == pid)
						break;
					ptr = ptr->next;
				}
				// if never capture this process, should ignore its I/Os
				if (ptr == NULL)
					break;

				unique_pid = ptr->unique_id;
				target_builder = hash_str(filename,
						total_size - ALPS_DIVIDE) + ALPS_DIVIDE;

				if ((flag & 0x3) == 0) {
					msg.message_header = EVENT_OPEN_RDONLY;
				} else if ((flag & 0x3) == 1) {
					msg.message_header = EVENT_OPEN_WRONLY;
				} else {
					msg.message_header = EVENT_OPEN_RDWR;
				}

				//check the first read.
				//unique_fid = UNIQUE_FID(filename, rank);
				bucket = (unsigned long) fd % BUCKET_NUM;
				f_pre_ptr = &ptr->all_opened_files[bucket];
				f_ptr = f_pre_ptr->next;
				int exception = 0;

				while (f_ptr != NULL) {
					if (strcmp(f_ptr->file_name, filename) == 0) {
						exception = 1;
						printf(
								"[UNEXPECTED ERROR] Process %d open an opened file fd [%d] %s again\n",
								pid, fd, filename);
					}
					f_pre_ptr = f_ptr;
					f_ptr = f_ptr->next;
				}

				if (exception == 0) { //create open file entity if no exception happens
					f_ptr = (struct file_access *) malloc(
							sizeof(struct file_access));
					if (msg.message_header == EVENT_OPEN_RDONLY)
						f_ptr->open_flag = ACCESS_BIT_OPEN_RDONLY;
					if (msg.message_header == EVENT_OPEN_WRONLY)
						f_ptr->open_flag = ACCESS_BIT_OPEN_WRONLY;
					if (msg.message_header == EVENT_OPEN_RDWR)
						f_ptr->open_flag = ACCESS_BIT_OPEN_RDWR;

					strcpy(f_ptr->file_name, filename);
					f_ptr->fd = fd;
					f_ptr->next = NULL;
					f_pre_ptr->next = f_ptr;

					msg.pid = unique_pid;
					msg.ts1 = timestamp;
					strcpy(msg.execfile, filename);
					MPI_Send(&msg, 1, alps_message_type, target_builder, 0,
					MPI_COMM_WORLD);
				}

				break;

			case EVENT_CLOSE_OC:
				sscanf(p, "%d %lld %d\n", &pid, &timestamp, &fd);
				bucket = pid % BUCKET_NUM;
				ptr = all_alive_execs[bucket].next;
				while (ptr != NULL) {
					if (ptr->pid == pid)
						break;
					ptr = ptr->next;
				}
				// if never capture this process, should ignore its I/Os
				if (ptr == NULL)
					break;

				unique_pid = ptr->unique_id;

				bucket = (unsigned long) fd % BUCKET_NUM;
				f_pre_ptr = &ptr->all_opened_files[bucket];
				f_ptr = f_pre_ptr->next;

				while (f_ptr != NULL) {
					if (f_ptr->fd == fd) {
						target_builder = hash_str(f_ptr->file_name,
								total_size - ALPS_DIVIDE) + ALPS_DIVIDE;

						msg.pid = unique_pid;
						strcpy(msg.execfile, filename);

						if (f_ptr->open_flag == ACCESS_BIT_OPEN_RDWR) {
							msg.message_header = EVENT_CLOSE_RDWR;
							msg.ts1 = timestamp;
						}
						if (f_ptr->open_flag == ACCESS_BIT_OPEN_WRONLY) {
							msg.message_header = EVENT_CLOSE_WRONLY;
							msg.ts1 = timestamp;
						}
						if (f_ptr->open_flag == ACCESS_BIT_OPEN_RDONLY) {
							msg.message_header = EVENT_CLOSE_RDONLY;
							msg.ts1 = timestamp;
						}

						MPI_Send(&msg, 1, alps_message_type, target_builder, 0,
						MPI_COMM_WORLD);

						//delete it
						struct file_access *f_free = f_ptr;
						f_pre_ptr->next = f_ptr->next;
						f_ptr = f_ptr->next;
						free(f_free);
					}
					f_pre_ptr = f_ptr;
					if (f_ptr != NULL)
						f_ptr = f_ptr->next;
				}
				break;

			case EVENT_CLOSE_FL:
				sscanf(p, "%d %lld %d %lld %lld %lld %lld\n", &pid, &timestamp,
						&fd, &fr_ts, &lr_ts, &fw_ts, &lw_ts);

				bucket = pid % BUCKET_NUM;
				ptr = all_alive_execs[bucket].next;
				while (ptr != NULL) {
					if (ptr->pid == pid)
						break;
					ptr = ptr->next;
				}
				// if never capture this process, should ignore its I/Os
				if (ptr == NULL)
					break;

				unique_pid = ptr->unique_id;

				bucket = (unsigned long) fd % BUCKET_NUM;
				f_pre_ptr = &ptr->all_opened_files[bucket];
				f_ptr = f_pre_ptr->next;

				while (f_ptr != NULL) {
					if (f_ptr->fd == fd) {
						target_builder = hash_str(f_ptr->file_name,
								total_size - ALPS_DIVIDE) + ALPS_DIVIDE;

						msg.pid = unique_pid;
						strcpy(msg.execfile, filename);

						if (fr_ts != 0) { //send out first read
							msg.ts1 = fr_ts;
							msg.message_header = EVENT_FIRST_READ;
							MPI_Send(&msg, 1, alps_message_type, target_builder,
									0,
									MPI_COMM_WORLD);
						}
						if (fw_ts != 0) { //send out first write
							msg.ts1 = fw_ts;
							msg.message_header = EVENT_FIRST_READ;
							MPI_Send(&msg, 1, alps_message_type, target_builder,
									0,
									MPI_COMM_WORLD);
						}

						if (lr_ts != 0) { //send out last read
							msg.ts1 = lr_ts;
							msg.message_header = EVENT_LAST_READ;
							MPI_Send(&msg, 1, alps_message_type, target_builder,
									0,
									MPI_COMM_WORLD);
						}
						if (lw_ts != 0) { //send out last write
							msg.ts1 = lw_ts;
							msg.message_header = EVENT_LAST_READ;
							MPI_Send(&msg, 1, alps_message_type, target_builder,
									0,
									MPI_COMM_WORLD);
						}

						if (f_ptr->open_flag == ACCESS_BIT_OPEN_RDWR) {
							msg.message_header = EVENT_CLOSE_RDWR;
							msg.ts1 = timestamp;
						}
						if (f_ptr->open_flag == ACCESS_BIT_OPEN_WRONLY) {
							msg.message_header = EVENT_CLOSE_WRONLY;
							msg.ts1 = timestamp;
						}
						if (f_ptr->open_flag == ACCESS_BIT_OPEN_RDONLY) {
							msg.message_header = EVENT_CLOSE_RDONLY;
							msg.ts1 = timestamp;
						}

						MPI_Send(&msg, 1, alps_message_type, target_builder, 0,
						MPI_COMM_WORLD);

						//delete it
						struct file_access *f_free = f_ptr;
						f_pre_ptr->next = f_ptr->next;
						f_ptr = f_ptr->next;
						free(f_free);
					}
					f_pre_ptr = f_ptr;
					if (f_ptr != NULL)
						f_ptr = f_ptr->next;
				}
				break;

			}
			//printf("AGGREGATOR GETS LINE: %s END\n", line);
		}
		fclose(pipe_fd);

	} else {
		char db_file[32], db_env[32];
		sprintf(db_file, "%s%d.file", "db", rank);
		sprintf(db_env, "%s", "/tmp/gdb");
		int ret = init_db(db_file, db_env);
		if (ret != 0)
			return -1;

		struct alps_version all_accessed_files[BUCKET_NUM];
		for (int i = 0; i < BUCKET_NUM; i++) {
			memset(&all_accessed_files[i], 0, sizeof(struct alps_version));
		}

		while (1) {
			alps_message recv;
			MPI_Status status;

			MPI_Recv(&recv, 1, alps_message_type,
			MPI_ANY_SOURCE, 0,
			MPI_COMM_WORLD, &status);
			//MPI_Send(buffer, 256, MPI_CHAR, status.MPI_SOURCE, 0, MPI_COMM_WORLD);

			if (recv.message_header == EVENT_PROCESS_CREAT) {
#ifdef DEBUG_BUILDER_LEVEL_1
				printf(
						"[4] [BUILDER] Process %lld create Child %lld at %lld Start\n",
						GET_PID(recv.pid), GET_PID(recv.child_pid), recv.ts1);
#endif
				builder_process_create_handler(recv);
#ifdef DEBUG_BUILDER_LEVEL_2
				printf(
						"[5] [BUILDER] Process %lld create Child %lld at %lld End\n",
						GET_PID(recv.pid), GET_PID(recv.child_pid), recv.ts1);
#endif
			} else if (recv.message_header == EVENT_PROCESS_EXIT) {
#ifdef DEBUG_BUILDER_LEVEL_1
				printf(
						"[4] [BUILDER] Process %lld Start at %lld. Exit at %lld with Metadata: execname (%s), argstr(%s), env(%s), retstr(%s), execfile(%s) Start\n",
						GET_PID(recv.pid), recv.ts1, recv.ts2, recv.execname,
						recv.argstr, recv.env, recv.retstr, recv.execfile);
#endif
				builder_process_exit_handler(recv);
#ifdef DEBUG_BUILDER_LEVEL_2
				printf(
						"[5] [BUILDER] Process %lld Start at %lld. Exit at %lld with Metadata: execname (%s), argstr(%s), env(%s), retstr(%s), execfile(%s) End\n",
						GET_PID(recv.pid), recv.ts1, recv.ts2, recv.execname,
						recv.argstr, recv.env, recv.retstr, recv.execfile);
#endif
			} else {
#ifdef DEBUG_BUILDER_LEVEL_1
				printf("[4] [BUILDER Process %lld %d %s at %lld Start\n",
						GET_PID(recv.pid), recv.message_header, recv.execfile, recv.ts1);
#endif
				builder_file_event_handler(recv.message_header, recv, rank,
						all_accessed_files);
#ifdef DEBUG_BUILDER_LEVEL_2
				printf("[5] [BUILDER Process %lld %d %s at %lld End\n",
						GET_PID(recv.pid), recv.message_header, recv.execfile, recv.ts1);
#endif
			}
		}
	}

	MPI_Finalize();
	return 0;
}
