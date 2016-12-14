all: web_proxy

web_proxy: main.o cache.o
	gcc -o web_proxy main.o cache.o -lpthread

main.o: main.c cache.h
	gcc -c main.c

cache.o: cache.c cache.h
	gcc -c cache.c

clean:
	rm -f *.o
	rm -f web_proxy
