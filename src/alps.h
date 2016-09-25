#ifndef _ALPS_H
#define _ALPS_H

struct alps_exec {
	int unique_id;

	int pid, parent_pid;
	long long start_ts, end_ts;
	char execname[128], argstr[128], env[128], args[128], retstr[128], execfile[128];
	int isExit, isRE;

	struct alps_exec *next;
}

struct alps_exec_file {

	long long ts;
	int alps_exec_id;
	char filename[256];
	int access_type; // 0:READ; 1:WRITE; 2:READWRITE; 3:OPEN; 4:CLOSE;
}

typedef struct alps_message_s {
	int message_header;
	long long pid, child_pid;
	long long ts1, ts2;
	char execname[128], argstr[128], env[128], retstr[128], execfile[128];
	int fd, flag;
} alps_message;

#endif