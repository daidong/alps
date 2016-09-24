#include "utils.h"

unsigned long hash(char *str, unsigned long starter,  unsigned long total){
										 
	unsigned long hash = 5381;
	int c;
	
	while (c = *str++)
		hash = ((hash << 5) + hash) + c; /* hash * 33 + c */
	
	return (hash + starter) % total;
	
}

void parse_process_create(char *p, unsigned int *child_pid, unsigned long *ts, unsigned int *parent_pid){
	//skip the first char
	p = p + 1;

	while ((*p) != 0){
		
		while ( (*p) != 3 ){

		}
	}
}
