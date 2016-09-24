#include "utils.h"

unsigned long hash(char *str, unsigned long starter,  unsigned long total){
										 
	unsigned long hash = 5381;
	int c;
	
	while (c = *str++)
		hash = ((hash << 5) + hash) + c; /* hash * 33 + c */
	
	return (hash + starter) % total;
	
}