#ifndef _ALPS_H
#define _ALPS_H

#define EVENT_PROCESS_CREAT 97
#define EVENT_PROCESS_EXIT 98
#define EVENT_OPEN_RDONLY 101
#define EVENT_OPEN_RDWR 102
#define EVENT_CLOSE 103
#define EVENT_READ 112
#define EVENT_WRITE 113
#define EVENT_FIRST_READ 114
#define EVENT_FIRST_WRITE 115
#define EVENT_LAST_RW 116

struct alps_exec {
	int unique_id;
	int pid, parent_pid;
	long long start_ts, end_ts;
	char execname[128], argstr[128], env[128], args[128], retstr[128], execfile[128];
	int isExit, isRE;
	char first_read[256], first_write[256], last_read[256], last_write[256];
	int first_read_ts, first_write_ts, last_read_ts, last_write_ts;
	struct alps_exec *next;
};

struct alps_exec_file {
	long long ts;
	int alps_exec_id;
	char filename[256];
	int access_type; // 0:READ; 1:WRITE; 2:READWRITE; 3:OPEN; 4:CLOSE;
};

typedef struct alps_message_s {
	int message_header;
	long long pid, child_pid;
	long long ts1, ts2;
	char execname[128], argstr[128], env[128], retstr[128], execfile[128];
	int fd, flag;
} alps_message;

#endif