#ifndef _UTILS_H_

unsigned long long hash_file(char *str);
unsigned long hash_str(char *str, int total);
unsigned long hash_long(long long pid, int total);
unsigned long hash_int(int fd, int total);

#endif
