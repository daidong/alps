#include "utils.h"

int hash_file(char *str){
	unsigned long hash = 5381;
	int c;
	
	while (c = *str++)
		hash = ((hash << 5) + hash) + c; /* hash * 33 + c */
	
	return hash;
}

int hash_str(char *str, int total){
										 
	unsigned long hash = 5381;
	int c;
	
	while (c = *str++)
		hash = ((hash << 5) + hash) + c; /* hash * 33 + c */
	
	return hash % total;
}

int hash_long(long long pid, int total) {
	return pid % total;
}

