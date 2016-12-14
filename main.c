#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netdb.h>
#include <errno.h>
#include <pthread.h>
#include <semaphore.h>
#include "cache.h"

#define MAX_BUFFER_SIZE 1000

char source[15], target[15];
int dc_flag;

cache_list cache;
int readcount;
sem_t mutex, wrt;

void error(char* msg)
{
	perror(msg);
	exit(0);
}

void parsing_uri(char *hostname, char *path, int *port, char *uri)
{
	int flag, i;
	char *temp;

	for(i = 7; i < strlen(uri); i++)
	{
		if(uri[i] == ':')
		{
			flag = 1;
			break;
		}
		else if(uri[i] == '/')
			break;
	}

	if(flag == 1)
	{
	//	temp = strpbrk(strpbrk(uri, ":") + 1, ":");
	//	sscanf(temp, "%d %s", port, path);
		sscanf(uri, "http://%[^:]s", hostname);
		sscanf(uri, "http://%*[^:]%d %s", port, path);
	}
	else
	{
		sscanf(uri, "http://%[^/]s/", hostname);
		sscanf(uri, "http://%*[^/]%s", path);
		*port = 80;
	}
}

void str_replace(char *buf, char *sour, char *targ)
{
	char *p;

	do
	{
		p = strstr(buf, sour);
		if(p != NULL)
			memcpy(p, targ, targ_len);
	}while(p != NULL);
}

void *proxy_server(void *sockfd)
{
	struct sockaddr_in host_addr;
	struct hostent* host;
	int client_sockfd, proxy_sockfd, n, port;
	char buffer[MAX_BUFFER_SIZE + 10], method[10], uri[150], ver[10], hostname[30], path[120];

	char cache_buffer[MAX_OBJECT_SIZE];
	int cbuf_len, flag;

	client_sockfd = *((int *) sockfd);
	free(sockfd);

	bzero((char*)buffer, MAX_BUFFER_SIZE);
	recv(client_sockfd,buffer, MAX_BUFFER_SIZE, 0);
//	printf("%s\n", buffer);
	sscanf(buffer, "%s %s %s", method, uri, ver);

	if(((strncmp(method, "GET", 3) == 0)) && ((strncmp(ver, "HTTP/1.1", 8) == 0)))
	{
		parsing_uri(hostname, path, &port, uri);
		printf("\nhost = %s\npath = %s\nPort = %d\n", hostname, path, port);

		host = gethostbyname(hostname);
		bzero((char*)&host_addr, sizeof(host_addr));
		host_addr.sin_port = htons(port);
		host_addr.sin_family = AF_INET;
		bcopy((char*)host->h_addr, (char*)&host_addr.sin_addr.s_addr, host->h_length);

		proxy_sockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		if(connect(proxy_sockfd, (struct sockaddr*)&host_addr, sizeof(struct sockaddr)) < 0)
			error("connect error");
		
		// data_change(delete gzip)
		if(dc_flag)
			str_replace(buffer, "gzip", "    ");

		// file_caching(reader - writer problem)
		sem_wait(&mutex);
		readcount++;
		if(readcount == 1)
			sem_wait(&wrt);
		sem_post(&mutex);
		
		bzero((char *)cache_buffer, MAX_OBJECT_SIZE);
		cbuf_len = find_cache(cache_buffer, cache, uri);
		sem_wait(&mutex);
		readcount --;
		if(readcount == 0)
			sem_post(&wrt);
		sem_post(&mutex);
		
		if(cbuf_len > 0)
		{
			printf("\nSend Cacheing\nURI : %s\n", uri);
			send(proxy_sockfd, cache_buffer, cbuf_len, 0);
		}
		
		//printf("\n%s\n", buffer);
		n = send(proxy_sockfd, buffer, strlen(buffer), 0);

		if(n < 0)
			error("request send error");
		else{
			bzero((char *)cache_buffer, MAX_OBJECT_SIZE);
			cbuf_len = 0;
			flag = 1;
			do
			{
				bzero((char*)buffer, MAX_BUFFER_SIZE);
				n = recv(proxy_sockfd, buffer, MAX_BUFFER_SIZE, 0);

				// data_change
				if(dc_flag)
					str_replace(buffer, source, target);

				if(n > 0)
					send(client_sockfd, buffer, n, 0);

				// file_caching
				if(cbuf_len + n < MAX_OBJECT_SIZE && flag)
				{
					memcpy(cache_buffer + cbuf_len, buffer, n);
					cbuf_len += n;
				}
				else
					flag = 0;

			}while(n > 0);

			// file_caching
			if(flag)
			{
				sem_wait(&wrt);
				insert_cache(cache, uri, cache_buffer, cbuf_len);
				sem_post(&wrt);
			}
		}
	}
	else
		send(client_sockfd, "400 : BAD REQUEST\nONLY HTTP REQUESTS ALLOWED", 18, 0);
	
	close(client_sockfd);
	return NULL;
}

int main(int argc,char* argv[])
{
	struct sockaddr_in addr_in, client_addr, server_addr;
	int server_sockfd, *client_sockfd, clilen;
	pthread_t thread_t;

	if(argc == 3)
	{
		dc_flag = 1;
		memcpy(source, argv[1], strlen(argv[1]));
		memcpy(target, argv[2], strlen(argv[2]));
	}

	cache = (cache_list) malloc(sizeof(struct cache_list));
	cache->size = 0;
	cache->first = NULL;

	readcount = 0;
	sem_init(&mutex, 0, 1);
	sem_init(&wrt, 0, 1);

	printf("\n*****WELCOME TO PROXY SERVER*****\n");

	bzero((char*)&server_addr, sizeof(server_addr));
	bzero((char*)&client_addr, sizeof(client_addr));

	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(8080);
	server_addr.sin_addr.s_addr = INADDR_ANY;

	server_sockfd = socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);
	if(server_sockfd < 0)
		error("socket init error");

	if(bind(server_sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0)
		error("bind error");

	if(listen(server_sockfd, 50) < 0)
		error("listen error");

	clilen = sizeof(client_addr);
	while(1)
	{
		client_sockfd = malloc(sizeof(int));
		if(client_sockfd == NULL)
			continue;

		*client_sockfd = accept(server_sockfd, (struct sockaddr*)&client_addr, (unsigned int*)&clilen);

		if(*client_sockfd > 0)
			pthread_create(&thread_t, NULL, proxy_server, client_sockfd);
	}

	return 0;
}
