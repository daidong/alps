#include "stdio.h"
#include "store.h"

void main(int argc, char **argv){

	int rank = 1;
	char db_file[32], db_env[32];
	sprintf(db_file, "%s%d.file", "db", rank);
	sprintf(db_env, "%s", "/tmp/gdb");

	int ret = init_db(db_file, db_env);
	if (ret != 0)
		return;

	iterate_json();
	close_db();
}

