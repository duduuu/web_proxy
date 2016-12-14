#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <assert.h>

#define MAX_CACHELIST_SIZE 100000
#define MAX_OBJECT_SIZE 10000

struct cache_list{
	int size;
	struct cache_object *first;
};

struct cache_object{
	int size;
	char *uri;
	char *buf;
	time_t timestamp;
	struct cache_object *prev;
	struct cache_object *next;
};

typedef struct cache_list * cache_list;
typedef struct cache_object * cache_object;


void insert_cache(cache_list C, char *uri, char *buf, int len);
int find_cache(char *buf, cache_list C, char *uri);
void remove_cache(cache_list C, cache_object W);