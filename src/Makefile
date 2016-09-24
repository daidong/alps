all: alps
	
alps: alps.c store.c dbkey.c
	mpicc -O2 -std=gnu99 -o alps alps.c store.c dbkey.c utils.c -ldb -g 

clean:
	rm -f alps
