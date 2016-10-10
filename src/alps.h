#ifndef _ALPS_H
#define _ALPS_H

#define BUCKET_NUM 128
#define STR_MAX_LEN 256 //file name size; argstr size; path size;

//#define DEBUG_AGG_LEVEL_1
//#define DEBUG_AGG_LEVEL_2
//#define DEBUG_BUILDER_LEVEL_1
//#define DEBUG_BUILDER_LEVEL_2

#define ALPS_DIVIDE 1

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
#define EVENT_CLOSE_RDWR 107
#define EVENT_READ_START 108
#define EVENT_READ_END 109
#define EVENT_WRITE_START 110
#define EVENT_WRITE_END 111

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

const static int ACCESS_BIT_LAST_WRITE = 0;
const static int ACCESS_BIT_LAST_READ = 1;
const static int ACCESS_BIT_FIRST_WRITE = 2;
const static int ACCESS_BIT_FIRST_READ = 3;
const static int ACCESS_BIT_OPEN_RDWR = 4;
const static int ACCESS_BIT_OPEN_WRONLY = 5;
const static int ACCESS_BIT_OPEN_RDONLY = 6;

const static int CLOCK_SKEW = 100; //maximal clock skew is 100ms
const static int TOLERABLE_DELAY = 1000; //maximal tolerable delay is 1000ms = 1s

struct file_access {
	char file_name[STR_MAX_LEN];
	long long ts;
	int flag; //6 OPEN_RDONLY; 5 OPEN_WRONLY; 4 OPEN_RDWR; 3 FIRST_READ; 2 FIRST_WRITE; 1 LAST_READ; 0 LAST_WRITE
	struct file_access *next;
};

struct alps_exec {
	unsigned long long unique_id;
	int pid, parent_pid;
	long long start_ts, end_ts;
	char execname[STR_MAX_LEN], argstr[STR_MAX_LEN], env[STR_MAX_LEN];
	char args[STR_MAX_LEN], retstr[STR_MAX_LEN], execfile[STR_MAX_LEN];
	struct file_access all_opened_files[BUCKET_NUM];
	struct alps_exec *next;
};

struct alps_exec_file {
	long long ts;
	int alps_exec_id;
	char filename[STR_MAX_LEN];
	int access_type; // 0:READ; 1:WRITE; 2:READWRITE; 3:OPEN; 4:CLOSE;
};

struct alps_version {
	unsigned long long unique_fid;
	unsigned long long unique_pid;
	long long timestamp;

	int version;
	int event;

	struct alps_version *hash_next;
	struct alps_version *version_next;
};

typedef struct alps_message_s {
	int message_header;
	unsigned long long pid, child_pid;
	long long ts1, ts2;
	char execname[STR_MAX_LEN], argstr[STR_MAX_LEN], env[STR_MAX_LEN], retstr[STR_MAX_LEN], execfile[STR_MAX_LEN];
	int fd, flag;
} alps_message;

#endif
