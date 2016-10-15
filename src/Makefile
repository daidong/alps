all: alps query json
	
alps: alps.c store.c dbkey.c alps.h store.h dbkey.h
	mpicc -O2 -std=gnu99 -o alps alps.c store.c dbkey.c utils.c -ldb -g 

query: query.c store.h store.c
	gcc -O2 -std=gnu99 -o query query.c store.c dbkey.c -ldb -g

json: json.c store.h store.c
	gcc -O2 -std=gnu99 -o json json.c store.c dbkey.c -ldb -g
	
clean:
	rm -f alps query json
