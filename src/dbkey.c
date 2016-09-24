#include <stdlib.h>
#include <string.h>

#include "dbkey.h"

DBKey* build(const DBT *a){
	DBKey *key = malloc(sizeof(DBKey));
	char *p = a->data;

	memcpy(&key->src.size, p, sizeof(u_int16_t));
	key->src.data = (char *) malloc(key->src.size);
	memcpy(key->src.data, p + sizeof(u_int16_t), key->src.size);

	memcpy(&key->dst.size, p + sizeof(u_int16_t) + key->src.size, sizeof(u_int16_t));
	key->dst.data = (char *) malloc(key->dst.size);
	memcpy(key->dst.data, p + sizeof(u_int16_t) * 2 + key->src.size, key->dst.size);

	memcpy(&key->ts, p + 2 * sizeof(u_int16_t) + key->src.size + key->dst.size, sizeof(u_int64_t));
	memcpy(&key->type, p + 2 * sizeof(u_int16_t) + key->src.size + key->dst.size + sizeof(u_int64_t), sizeof(u_int32_t));

	return key;
}

char *decompose(DBKey* key, u_int32_t *size){
	u_int32_t total = key->src.size + sizeof(u_int16_t) * 2 + key->dst.size + sizeof(u_int32_t) + sizeof(u_int64_t);
	* size = total;
	char *p = malloc(total);
	memcpy(p, &key->src.size, sizeof(u_int16_t));
	memcpy(p + sizeof(u_int16_t), key->src.data, key->src.size);

	memcpy(p + sizeof(u_int16_t) + key->src.size, &key->dst.size, sizeof(u_int16_t));
	memcpy(p + 2 * sizeof(u_int16_t) + key->src.size, key->dst.data, key->dst.size);

	memcpy(p + 2 * sizeof(u_int16_t) + key->src.size + key->dst.size, &key->ts, sizeof(u_int64_t));
	memcpy(p + 2 * sizeof(u_int16_t) + key->src.size + key->dst.size + sizeof(u_int64_t), &key->type, sizeof(u_int32_t));

	return p;
}

void pprint(const DBKey* key){
	printf("DBKey: ");
	for (int i = 0; i < key->src.size; i++)
		printf("%c", *(key->src.data + i));
	printf(" -> ");
	for (int i = 0; i < key->dst.size; i++)
		printf("%c", *(key->dst.data + i));
	printf(" [%d][%ld]\n", key->type, key->ts);
}

u_int32_t key_size(const DBKey* key){
	return key->src.size + sizeof(u_int16_t) * 2 + key->dst.size + sizeof(u_int32_t) + sizeof(u_int64_t);
}

int compare_slice(Slice a, Slice b){
	if (a.size == b.size && memcmp(a.data, b.data, a.size) == 0)
		return 0;
	if (a.size != b.size)
		return (a.size - b.size);
	for (int i = 0; i < a.size; i++){
		if (memcmp(a.data + i, b.data + i, sizeof(char)) != 0)
			return memcmp(a.data + i, b.data + i, sizeof(char));
	}
	return 0;
}

int compare_dbkey_v4(DB *dbp, const DBT *a, const DBT *b){
	DBKey* _a = build(a);
	DBKey* _b = build(b);

	if (compare_slice(_a->src, _b->src) != 0)
		return compare_slice(_a->src, _b->src);

	if (_a->type != _b->type)
		return _a->type - _b->type;

	if (compare_slice(_a->dst, _b->dst) != 0)
		return compare_slice(_a->dst, _b->dst);

	if (_a->ts != _b->ts)
		return _b->ts - _a->ts;

	return 0;
}

int compare_dbkey_v6(DB *dbp, const DBT *a, const DBT *b, size_t *locp){
	locp = NULL;
	DBKey* _a = build(a);
	DBKey* _b = build(b);

	if (compare_slice(_a->src, _b->src) != 0)
		return compare_slice(_a->src, _b->src);

	if (_a->type != _b->type)
		return _a->type - _b->type;

	if (compare_slice(_a->dst, _b->dst) != 0)
		return compare_slice(_a->dst, _b->dst);

	if (_a->ts != _b->ts)
		return _b->ts - _a->ts;

	return 0;
}
