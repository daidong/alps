#ifndef _CONST_H
#define _CONST_H

#define BUCKET_NUM 128
#define STR_MAX_LEN 256 //file name size; argstr size; path size;

#define MAX_NODE 1000
#define MAX_STR 1024

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
#define EVENT_CLOSE_OC 104
#define EVENT_CLOSE_FL 105
#define EVENT_CLOSE_RDONLY 106
#define EVENT_CLOSE_WRONLY 107
#define EVENT_CLOSE_RDWR 108
#define EVENT_READ_START 109
#define EVENT_READ_END 110
#define EVENT_WRITE_START 111
#define EVENT_WRITE_END 112

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

const static int GROUP_PROCESS = 0;
const static int GROUP_FILE = 1;


#endif
