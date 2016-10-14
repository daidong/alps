#include "utils.h"

unsigned long long hash_file(char *str){
	unsigned long long hash = 5381;
	int c;
	
	while (c = *str++)
		hash = ((hash << 5) + hash) + c; /* hash * 33 + c */
	
	return hash;
}

unsigned long hash_str(char *str, int total){
										 
	unsigned long hash = 5381;
	int c;
	
	while (c = *str++)
		hash = ((hash << 5) + hash) + c; /* hash * 33 + c */
	
	return hash % total;
}

unsigned long hash_long(long long pid, int total) {
	return pid % total;
}

unsigned long hash_int(int fd, int total){
	return fd % total;
}

