#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "store.h"
#include "dbkey.h"

/* global variables */

static DB_ENV *Env;
static DB *dbp;

int init_db(char *DB_FILE, char *ENV_DIR) {
	//printf("DB_FILE: %s, ENV_DIR: %s\n", DB_FILE, ENV_DIR);
	u_int32_t flags;
	u_int32_t env_flags;
	int ret;

	ret = db_env_create(&Env, 0);
	if (ret != 0) {
		fprintf(stderr, "Error creating env handle: %s\n", db_strerror(ret));
		return -1;
	}

	/* Open the environment. */
	env_flags = DB_CREATE | /* If the environment does not exist, create it. */
	DB_INIT_MPOOL; /* Initialize the in-memory cache. */
	ret = Env->open(Env, ENV_DIR, env_flags, 0);
	if (ret != 0) {
		fprintf(stderr, "Environment open failed: %s", db_strerror(ret));
		return -1;
	}

	ret = db_create(&dbp, Env, 0);
	if (ret != 0) {
		fprintf(stderr, "Error creating db handler: %s", db_strerror(ret));
		return -1;
	}

	dbp->set_bt_compare(dbp, compare_dbkey_v4); //set the comparator, must before open

	flags = DB_CREATE; //DB_EXCL, DB_RDONLY, DB_TRUNCATE; DB->get_open_flags()
	ret = dbp->open(dbp,
	NULL, DB_FILE,
	NULL, DB_BTREE, flags, 0);
	if (ret != 0) {
		fprintf(stderr, "My Database open failed: %s\n", db_strerror(ret));
		return -1;
	}
	return ret;
}

void close_db() {
	if (dbp != NULL)
		dbp->close(dbp, 0);
	if (Env != NULL)
		Env->close(Env, 0);
	//You can further remove the database
	//dbp->remove(dbp, dbf, NULL, 0);
}

void close_and_remove_db(const char *db_env, const char *db_file) {

	if (dbp != NULL)
		dbp->close(dbp, 0);

	char file_full_path[64];
	sprintf(file_full_path, "%s/%s", db_env, db_file);
	printf("db_file:%s\n", file_full_path);

	dbp->remove(dbp, file_full_path, NULL, 0);

	if (Env != NULL)
		Env->close(Env, 0);
}

int insert(char *src, u_int32_t ssize, char *dst, u_int32_t dsize,
		u_int32_t type, u_int64_t ts, char *val, u_int32_t vsize) {
	DBKey dbkey;
	Slice _a = { ssize, src };
	Slice _b = { dsize, dst };
	dbkey.src = _a;
	dbkey.dst = _b;
	dbkey.ts = ts;
	dbkey.type = type;

	DBT key, value;
	/* Zero out the DBTs before using them. */
	memset(&key, 0, sizeof(DBT));
	memset(&value, 0, sizeof(DBT));

	value.size = vsize;
	value.data = val;

	key.data = (void *) decompose(&dbkey, &key.size);
	/*
	 DBKey *check_dbkey = build(&key);
	 printf("src[%d]-dst[%d]-ts[%lld]-type[%d] : %.*s\n",
	 (*(int *)check_dbkey->src.data),
	 (*(int *)check_dbkey->dst.data),
	 (long long) check_dbkey->ts,
	 (int) check_dbkey->type,
	 (int) value.size, (char *)value.data);
	 */
	int ret = dbp->put(dbp, NULL, &key, &value, DB_NOOVERWRITE);
	if (ret == DB_KEYEXIST) {
		//dbp->err(dbp, ret, "Put failed because key already exists");
	}
	return ret;
}

char* get(char *src, u_int32_t ssize, char *dst, u_int32_t dsize,
		u_int32_t type, u_int64_t ts, u_int32_t *vsize) {
	DBKey dbkey;
	Slice _a = { ssize, src };
	Slice _b = { dsize, dst };
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

int del(char *src, u_int32_t ssize, char *dst, u_int32_t dsize, u_int32_t type,
		u_int64_t ts) {
	DBKey dbkey;
	Slice _a = { ssize, src };
	Slice _b = { dsize, dst };
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

void iterate_print() {
	DBC *dbcp;
	DBT key, data;
	int close_db, close_dbc, ret;

	close_db = close_dbc = 0;

	/* Acquire a cursor for the database. */
	if ((ret = dbp->cursor(dbp, NULL, &dbcp, 0)) != 0) {
		dbp->err(dbp, ret, "DB->cursor");
		return;
	}

	/* Initialize the key/data return pair. */
	memset(&key, 0, sizeof(DBT));
	memset(&data, 0, sizeof(DBT));

	/* Walk through the database and print out the key/data pairs. */
	while ((ret = dbcp->c_get(dbcp, &key, &data, DB_NEXT)) == 0) {
		DBKey *dbkey = build(&key);

		printf("src[%llu]-dst[%llu]-ts[%lld]-type[%d] : %.*s\n",
				((*(unsigned long long *) dbkey->src.data) & 0xFFFF),
				((*(unsigned long long *) dbkey->dst.data) & 0xFFFF),
				(long long) dbkey->ts, dbkey->type, (int) data.size,
				(char *) data.data);
	}

	if (ret != DB_NOTFOUND) {
		dbp->err(dbp, ret, "DBcursor->get");
		return;
	}
}

void iterate_json() {
	DBC *dbcp;
	DBT key, data;
	int close_db, close_dbc, ret;

	close_db = close_dbc = 0;

	/* Acquire a cursor for the database. */
	if ((ret = dbp->cursor(dbp, NULL, &dbcp, 0)) != 0) {
		dbp->err(dbp, ret, "DB->cursor");
		return;
	}

	/* Initialize the key/data return pair. */
	memset(&key, 0, sizeof(DBT));
	memset(&data, 0, sizeof(DBT));

	struct node header;
	memset(&header, 0, sizeof(struct node));
	struct node *pre_ptr, *ptr;

	int node_num = 0;
	int link_num = 0;

	/* Walk through the database and print out the nodes. */
	while ((ret = dbcp->c_get(dbcp, &key, &data, DB_NEXT)) == 0) {
		DBKey *dbkey = build(&key);

		unsigned long long src, short_src, dst, short_dst;

		src = (*(unsigned long long *) dbkey->src.data);
		short_src = src & 0xFFFF;
		dst = (*(unsigned long long *) dbkey->dst.data);
		short_dst = dst & 0xFFFF;

		long long ts = (long long) dbkey->ts;

		int type = dbkey->type;

		int length = data.size;
		char *str = (char *) malloc(sizeof(char) * length + 1);
		memcpy(str, data.data, length);
		*(str + length) = '\0';
		char *p = str;
		while (*p != '\0') {
			if (*p == '"')
				*p = '\'';
			p = p + 1;
		}

		if (type == EDGE_PARENT_CHILD || type == EDGE_CHILD_PARENT) {
			int src_exist = 0, dst_exist = 0;

			pre_ptr = &header;
			ptr = header.next;

			while (ptr != NULL) {
				if (ptr->uniqueId == (src + ts))
					src_exist = 1;
				if (ptr->uniqueId == (dst + ts))
					dst_exist = 1;
				pre_ptr = ptr;
				ptr = ptr->next;
			}
			if (src_exist == 0) {
				ptr = (struct node *) malloc(sizeof(struct node));
				ptr->version = ts;
				ptr->uniqueId = src + ptr->version;
				ptr->next = NULL;
				ptr->group = GROUP_PROCESS; //process
				sprintf(ptr->name, "%llu ", short_src);
				pre_ptr->next = ptr;
			}
			if (dst_exist == 0) {
				ptr = (struct node *) malloc(sizeof(struct node));
				ptr->version = ts;
				ptr->uniqueId = dst + ptr->version;
				ptr->next = NULL;
				ptr->group = GROUP_PROCESS; //process
				sprintf(ptr->name, "%llu ", short_dst);
				pre_ptr->next = ptr;
			}
		}
		if (type == EDGE_ATTR) {
			pre_ptr = &header;
			ptr = header.next;

			while (ptr != NULL) {
				if (ptr->uniqueId == (src + ts)) {
					if (short_dst == ATTR_EXEC_NAME)
						strcat(ptr->name, str);
					if (short_dst == ATTR_ARG_STR)
						strcat(ptr->name, str);
					if (short_dst == ATTR_EXEC_FILE) {
						strcat(ptr->name, str);
					}
					break;
				}
				pre_ptr = ptr;
				ptr = ptr->next;
			}
			if (ptr == NULL) {
				ptr = (struct node *) malloc(sizeof(struct node));
				ptr->version = ts;
				ptr->uniqueId = src + ptr->version;
				ptr->next = NULL;
				ptr->group = GROUP_PROCESS; //process
				sprintf(ptr->name, "%llu ", short_src);
				pre_ptr->next = ptr;
			}
		}

		if (type == EDGE_WRITE_BY || type == EDGE_READ_BY) {
			pre_ptr = &header;
			ptr = header.next;

			while (ptr != NULL) {
				if (ptr->uniqueId == (src + ts) && ptr->version == ts) {
					strcpy(ptr->name, str);
					char ver[32] = { 0 };
					sprintf(ver, " VERSION: %lld", ts);
					strcat(ptr->name, ver);
					break;
				}
				pre_ptr = ptr;
				ptr = ptr->next;
			}
			if (ptr == NULL) {
				ptr = (struct node *) malloc(sizeof(struct node));
				ptr->version = ts;
				ptr->uniqueId = src + ptr->version;
				ptr->next = NULL;
				ptr->group = GROUP_FILE; //process
				strcpy(ptr->name, str);
				char ver[32] = { 0 };
				sprintf(ver, " VERSION: %lld", ts);
				strcat(ptr->name, ver);
				pre_ptr->next = ptr;
			}
		}

		if (type != EDGE_ATTR) {
			link_num += 1;
		}
	}

	pre_ptr = &header;
	ptr = header.next;
	while (ptr != NULL){
		node_num += 1;
		ptr = ptr->next;
	}

	pre_ptr = &header;
	ptr = header.next;
	printf("{\"nodes\":[\n");
	int p_idx = 0;
	while (ptr != NULL) {
		printf("\t{\"id\":\"%llu\", \"group\":%d, \"name\":\"%s\"}", ptr->uniqueId, ptr->group, ptr->name);
		if (p_idx != (node_num - 1))
			printf(",");
		printf("\n");
		p_idx ++;
		ptr = ptr->next;
	}
	printf("],");

	if ((ret = dbcp->close(dbcp)) != 0) {
		dbp->err(dbp, ret, "DB->cursor");
		return;
	}

	/* Again, acquire a cursor for the database. */
	if ((ret = dbp->cursor(dbp, NULL, &dbcp, 0)) != 0) {
		dbp->err(dbp, ret, "DB->cursor");
		return;
	}

	printf("\"links\":[\n");

	int l_idx = 0;
	/* Walk through the database and print out the links */
	while ((ret = dbcp->c_get(dbcp, &key, &data, DB_NEXT)) == 0) {
		DBKey *dbkey = build(&key);

		unsigned long long src = (*(unsigned long long *) dbkey->src.data);
		unsigned long long dst = (*(unsigned long long *) dbkey->dst.data);
		long long ts = (long long) dbkey->ts;
		int type = dbkey->type;

		struct node *pre_ptr = &header;
		struct node *ptr = header.next;

		if (type != EDGE_ATTR) {
			if (type == EDGE_READ_BY || type == EDGE_WRITE_BY)
				src += ts;
			if (type == EDGE_WRITE || type == EDGE_READ)
				dst += ts;

			printf("\t{\"source\":\"%llu\",\"target\":\"%llu\",\"value\":%lld}", src, dst, ts);
			if (l_idx != (link_num - 1))
				printf(",");
			printf("\n");
			l_idx += 1;
		}
	}

	printf("]}\n");

	if (ret != DB_NOTFOUND) {
		dbp->err(dbp, ret, "DBcursor->get");
		return;
	}
}

