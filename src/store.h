#ifndef _STORE_H_

#include <db.h>
#include "const.h"

struct node{
	int group;
	unsigned long long uniqueId;
	long long version;
	int print_id;
	char name[MAX_STR];
	struct node *next;
};

int init_db(char *db_file, char *env_dir);
void close_db();
int insert(char *src, u_int32_t ssize, char *dst, u_int32_t dsize, u_int32_t type, u_int64_t ts, char *val, u_int32_t vsize);
char* get(char *src, u_int32_t ssize, char *dst, u_int32_t dsize, u_int32_t type, u_int64_t ts, u_int32_t *vsize);
int del(char *src, u_int32_t ssize, char *dst, u_int32_t dsize, u_int32_t type, u_int64_t ts);
void iterate_print();
void iterate_json();
void close_and_remove_db(const char *db_env, const char *db_file);

#endif
