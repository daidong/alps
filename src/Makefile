all: alps
	
alps: alps.c store.c dbkey.c
	mpicc -O2 -std=gnu99 -o alps alps.c store.c dbkey.c utils.c -ldb -g 

test: test.c
	gcc -O2 -std=gnu99 -o test.o test.c store.c dbkey.c -ldb -g

clean:
	rm -f alps
