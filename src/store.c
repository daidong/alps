#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "store.h"
#include "dbkey.h"

/* global variables */

static DB_ENV *Env;
static DB *dbp;

int init_db(char *DB_FILE, char *ENV_DIR){
	printf("DB_FILE: %s, ENV_DIR: %s\n", DB_FILE, ENV_DIR);
	u_int32_t flags;
	u_int32_t env_flags;
	int ret;

	ret = db_env_create(&Env, 0);
	if (ret != 0) {
		fprintf(stderr, "Error creating env handle: %s\n", db_strerror(ret));
		return -1;
	}

	/* Open the environment. */
	env_flags = DB_CREATE |    /* If the environment does not exist, create it. */
				DB_INIT_MPOOL; /* Initialize the in-memory cache. */
	ret = Env->open(Env,
					  ENV_DIR,
					  env_flags,
					  0);
	if (ret != 0) {
		fprintf(stderr, "Environment open failed: %s", db_strerror(ret));
		return -1;
	}

	ret = db_create(&dbp, Env, 0);
	if (ret != 0) {
		fprintf(stderr, "Error creating db handler: %s", db_strerror(ret));
		return -1;
	}

	dbp->set_bt_compare(dbp, compare_dbkey_v6); //set the comparator, must before open

	flags = DB_CREATE; //DB_EXCL, DB_RDONLY, DB_TRUNCATE; DB->get_open_flags()
	ret = dbp->open(dbp,
					NULL,
					DB_FILE,
					NULL,
					DB_BTREE,
					flags,
					0);
	if (ret != 0) {
		fprintf(stderr, "Database open failed: %s", db_strerror(ret));
		return -1;
	}
	return ret;
}

void close_db(){
	if (dbp != NULL)
		dbp->close(dbp, 0);
	if (Env != NULL)
		Env->close(Env, 0);
	//You can further remove the database
	//dbp->remove(dbp, dbf, NULL, 0);
}

int insert(char *src, u_int32_t ssize, char *dst, u_int32_t dsize, u_int32_t type, u_int64_t ts, char *val, u_int32_t vsize){
	DBKey dbkey;
	Slice _a = {ssize, src};
	Slice _b = {dsize, dst};
	dbkey.src = _a;
	dbkey.dst = _b;
	dbkey.ts = ts;
	dbkey.type = type;
	int ret;

	DBT key, value;
	/* Zero out the DBTs before using them. */
	memset(&key, 0, sizeof(DBT));
	memset(&value, 0, sizeof(DBT));

	key.data = decompose(&dbkey, &key.size);

	value.size = vsize;
	value.data = val;

	ret = dbp->put(dbp, NULL, &key, &value, DB_NOOVERWRITE);
	if (ret == DB_KEYEXIST) {
	    dbp->err(dbp, ret,
	      "Put failed because key already exists");
	}
	return ret;
}

char* get(char *src, u_int32_t ssize, char *dst, u_int32_t dsize, u_int32_t type, u_int64_t ts, u_int32_t *vsize){
	DBKey dbkey;
	Slice _a = {ssize, src};
	Slice _b = {dsize, dst};
	dbkey.src = _a;
	dbkey.dst = _b;
	dbkey.ts = ts;
	dbkey.type = type;

	DBT key, value;
	/* Zero out the DBTs before using them. */
	memset(&key, 0, sizeof(DBT));
	memset(&value, 0, sizeof(DBT));

	key.data = decompose(&dbkey, &key.size);

	dbp->get(dbp, NULL, &key, &value, 0);
	*vsize = value.size;
	return value.data;
}

int del(char *src, u_int32_t ssize, char *dst, u_int32_t dsize, u_int32_t type, u_int64_t ts){
	DBKey dbkey;
	Slice _a = {ssize, src};
	Slice _b = {dsize, dst};
	dbkey.src = _a;
	dbkey.dst = _b;
	dbkey.ts = ts;
	dbkey.type = type;
	int ret;

	DBT key, value;
	/* Zero out the DBTs before using them. */
	memset(&key, 0, sizeof(DBT));
	memset(&value, 0, sizeof(DBT));

	key.data = decompose(&dbkey, &key.size);
	ret = dbp->del(dbp, NULL, &key, 0);
	return ret;
}
