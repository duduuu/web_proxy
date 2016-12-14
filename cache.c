#include "cache.h"

void insert_cache(cache_list Clist, char *uri, char *buf, int len)
{
	cache_object curr, LRU, Cobj;
	Cobj = (cache_object) malloc(sizeof(struct cache_object));
	Cobj->uri = (char *) malloc(sizeof(char) * strlen(uri) + 1);
	Cobj->buf = 	(char *) malloc(sizeof(char) * len);
	Cobj->size = len;
	Cobj->timestamp = time(NULL);
	strcpy(Cobj->uri, uri);
	memcpy(Cobj->buf, buf, len);
	Cobj->next = NULL;

	while(Clist->size + len > MAX_CACHELIST_SIZE) {
		curr = Clist->first;
		LRU = NULL;
		while(curr != NULL) {
			if(LRU == NULL || curr->timestamp < LRU->timestamp)
				LRU = curr;
			curr = curr->next;
		}
		remove_cache(Clist, LRU);
	}

	Cobj->next = Clist->first;
	Cobj->prev = NULL;
	if(Cobj->next != NULL)
		Cobj->next->prev = Cobj;
	Clist->first = Cobj;

	Clist->size += len;
}

void remove_cache(cache_list Clist, cache_object Cobj){
	if(Clist == NULL || Cobj == NULL) return;

	Clist->size -= Cobj->size;

	Cobj->prev->next = Cobj->next;
	if(Cobj->next != NULL)
		Cobj->next->prev = Cobj->prev;

	if (Cobj == NULL) return;
	free(Cobj->uri);
	free(Cobj->buf);
	free(Cobj);
}

int find_cache(char *buf, cache_list Clist, char *uri)
{
	cache_object curr = Clist->first;

	while(curr != NULL)
	{
		if(!strcmp(uri, curr->uri))
		{
			curr->timestamp = time(NULL);
			memcpy(buf, curr->buf, curr->size);
			return curr->size;
		}
		curr = curr->next;
	}
	return 0;
}