#ifndef _ALPS_H
#define _ALPS_H

#define EVENT_PROCESS_CREAT 97
#define EVENT_PROCESS_EXIT 98
#define EVENT_EXECVE_START 99
#define EVENT_EXECVE_END 100

#define EVENT_OPEN 101
#define EVENT_OPEN_RDONLY 101
#define EVENT_OPEN_WRONLY 102
#define EVENT_OPEN_RDWR 103
#define EVENT_CLOSE 104
#define EVENT_CLOSE_RDONLY 105
#define EVENT_CLOSE_WRONLY 106
#define EVENT_READ_START 107
#define EVENT_READ_END 108
#define EVENT_WRITE_START 109
#define EVENT_WRITE_END 110

#define EVENT_FIRST_READ 114
#define EVENT_FIRST_WRITE 115
#define EVENT_LAST_READ 116
#define EVENT_LAST_WRITE 117

#define EVENT_RDWR_ERROR 120

const static int EDGE_ATTR = 0;
const static int EDGE_PARENT_CHILD = 1;
const static int EDGE_WRITE = 2;
const static int EDGE_READ = 3;

const static int EDGE_CHILD_PARENT = 11;
const static int EDGE_WRITE_BY = 12;
const static int EDGE_READ_BY = 13;

const static int ATTR_EXEC_NAME = 100;
const static int ATTR_ARG_STR = 101;
const static int ATTR_ENV_STR = 102;
const static int ATTR_RET_STR = 103;
const static int ATTR_EXEC_FILE = 104;
const static int ATTR_START_TS = 105;
const static int ATTR_END_TS = 106;

const static int CLOCK_SKEW = 100; //maximal clock skew is 100ms
const static int TOLERABLE_DELAY = 1000; //maximal tolerable delay is 1000ms = 1s

struct alps_exec {
	long long unique_id;
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

struct alps_version {
	long long unique_fid;
	long long unique_pid;
	long long timestamp;

	int version;
	int event;

	struct alps_version *hash_next;
	struct alps_version *version_next;
};

typedef struct alps_message_s {
	int message_header;
	long long pid, child_pid;
	long long ts1, ts2;
	char execname[128], argstr[128], env[128], retstr[128], execfile[128];
	int fd, flag;
} alps_message;

#endif
