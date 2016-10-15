#ifndef _ALPS_H
#define _ALPS_H

#include "const.h"

struct file_access {
	char file_name[STR_MAX_LEN];
	int fd, open_flag;
	long long fr_ts, fw_ts, lr_ts, lw_ts;
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
