#ifndef _DBKEY_H_

#include <db.h>

typedef struct _slice_{
	u_int16_t size;
	char *data;
} Slice;

typedef struct _DBKey_ {
    Slice src;
	Slice dst;
	u_int64_t ts;
	u_int32_t type;
} DBKey;

int compare_dbkey_v4(DB *dbp, const DBT *a, const DBT *b);
int compare_dbkey_v6(DB *dbp, const DBT *a, const DBT *b, size_t *locp);
DBKey* build(const DBT *a);
char* decompose(DBKey* key, u_int32_t *size);
void pprint(const DBKey* key);
u_int32_t key_size(const DBKey* key);

#endif
