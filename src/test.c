#include "stdio.h"
#include "store.h"

void main(){

	int rank = 1;
	char db_file[32], db_env[32];
	sprintf(db_file, "%s%d.file", "/tmp/db", rank);
	sprintf(db_env, "%s%d.env", "/tmp/db", rank);
	init_db(db_file, db_env);

	iterate_print();
	/*
	const char *fifo_name = "/tmp/test.binary"; //this must be a local file
	FILE *pipe_fd;
	ssize_t read;
	size_t len = 0;
	pipe_fd = fopen(fifo_name, "r");
	char *line = NULL;

	int child_pid, pid, fd, flag;
	long long timestamp;
	char execname[128], argstr[128], args[256], env[128], filename[256], retstr[128];

	if (pipe_fd == NULL)
		return;
	
	while ((read = getline(&line, &len, pipe_fd)) != -1){
		char *p = line + 1;

		switch ((*line)){
				case 'a':
					sscanf(p, "%d %lld %d\n", &child_pid, &timestamp, &pid);
					printf("Create Process - Child-PID: %d, timestamp: %lld, Curr-PID: %d\n", child_pid, timestamp, pid);
					break;
				case 'b':
					sscanf(p, "%d %lld\n", &pid, &timestamp);
					printf("Exit Process - PID: %d, timestamp: %lld\n", pid, timestamp);
					break;
				case 'c':
					sscanf(p, "%d %lld %s %s %s %s %s\n", &pid, &timestamp, execname, argstr, env, filename, args);
					printf("Execve Start - PID: %d, timestamp: %lld, execname: %s, argstr: %s, env: %s, filename: %s, args: %s\n", pid, timestamp, execname, argstr, env, filename, args);
					break;
				case 'd':
					sscanf(p, "%d %lld %s %s\n", &pid, &timestamp, retstr, env);
					printf("Execve Return - PID: %d, timestamp: %lld, retstr: %s, env: %s\n", pid, timestamp, retstr, env);
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
	*/


	/*
	char str[1024];
	sprintf(str, "%d %d %d %d %d %s %s %s %s %s\n", 0, 1, 2, 3, 4, "execname", "argstr", "env", "filename", "retstr");

	sscanf(str, "%d %d %d %d %d %s %s %s %s %s\n", &child_pid, &timestamp, &pid, &fd, &flag, 
		execname, argstr, env, filename, retstr);

	printf("%d %d %d %d %d %s %s %s %s %s\n", child_pid, timestamp, pid, fd, flag, 
		execname, argstr, env, filename, retstr);
	*/
}