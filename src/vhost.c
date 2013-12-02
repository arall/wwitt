
#define _GNU_SOURCE 

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>
#include <pthread.h>
#include <mysql.h>
#include <arpa/inet.h>
#include <regex.h>
#include "pqueue.h"

// Maximum query size, 1MB
#define MAX_BUFFER_SIZE   (1024*1024)
#define NUM_WORKERS       4

MYSQL *mysql_conn_select;
MYSQL *mysql_conn_update;

typedef void (*callback_fn)(void*,int);

struct http_query {
	char * buffer;
	int received;
};
struct job_object {
	char url[8*1024];
	callback_fn callback;
};

static size_t curl_fwrite(void *buffer, size_t size, size_t nmemb, void *stream) {
	struct http_query * q = (struct http_query *)stream;

	if (q->buffer == NULL) q->buffer = malloc(size*nmemb + 32);
	else q->buffer = realloc(q->buffer, size*nmemb + q->received + 32);

	memcpy(&q->buffer[q->received], buffer, size*nmemb);
	q->received += size*nmemb;

	if (q->received > MAX_BUFFER_SIZE)
		return -1;  // Ops!

	return size*nmemb;
}

void * worker_thread(void * args) {
	printf("Spawning thread\n");
	struct pqueue * q = (struct pqueue*)args;
	struct job_object * job = pqueue_pop(q);
	while (job != NULL) {
		struct http_query hq;
		memset(&hq,0,sizeof(hq));

		CURL * curl = curl_easy_init();
		curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0);
		curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0);
		curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1);
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_fwrite);
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, &hq);
		curl_easy_setopt(curl, CURLOPT_URL, job->url);
	
		CURLcode res = curl_easy_perform(curl);
	
		if(CURLE_OK == res) {
			// Call the callback with the data fetched
			job->callback(hq.buffer,hq.received);
		}
	
		curl_easy_cleanup(curl);
		
		free(job);
		job = pqueue_pop(q);
	}
	
	printf("Ending thread\n");
}

struct re_pattern_buffer reg1, reg2;
void compile_regexp() {
	reg1.translate = 0;
	reg1.fastmap = 0;
	reg1.buffer = 0;
	reg1.allocated = 0;
	re_set_syntax(RE_SYNTAX_POSIX_EXTENDED);
	const char * pat1 = "<div[^<>]*class=\"sb_meta\"[^>]*>[^<>]*<cite>([^<]*)</cite>";
	re_compile_pattern(pat1, strlen(pat1), &reg1); 

	reg2.translate = 0;
	reg2.fastmap = 0;
	reg2.buffer = 0;
	reg2.allocated = 0;
	re_set_syntax(RE_SYNTAX_POSIX_EXTENDED);
	const char * pat2 = "IP:([0-9]+\\.[0-9]+\\.[0-9]+\\.[0-9]+)";
	re_compile_pattern(pat2, strlen(pat2), &reg2); 
}

// Parse BING result page
void bing_parser(void * buffer, int size) {
	char * cbuffer = (char*)buffer;
	cbuffer[size] = 0; // We have some space after the buffer

	char ipaddr[1024]; memset(ipaddr,0,sizeof(ipaddr));
	struct re_registers regs;
	if (re_search(&reg2,cbuffer,size,0,size,&regs)!=-1) {
		if (regs.start[1] >= 0) {
			memcpy(ipaddr,&cbuffer[regs.start[1]],regs.end[1]-regs.start[1]);
		}
	}

	int off = 0;
	while (re_search(&reg1,cbuffer,size,off,size,&regs) != -1) {
		if (regs.start[1] >= 0) {
			char temp[4096]; memset(temp,0,sizeof(temp));
			memcpy(temp,&cbuffer[regs.start[1]],regs.end[1]-regs.start[1]);
			printf("Match found %s %s\n",temp,ipaddr);
			off = regs.end[1];
		}
		else break;
	}
}
// Parse DT result page
void dt_parser(void * buffer, int sizei) {
	
}

void create_bing_url(char * url, unsigned int ip, int pagen) {
	struct in_addr in;
	in.s_addr = ip;
	char * ipaddr = inet_ntoa(in);
	sprintf(url, "http://www.bing.com/search?first=%d&q=IP:%s", pagen, ipaddr);
}
void create_dt_url(char * url, unsigned int ip) {
	struct in_addr in;
	in.s_addr = ip;
	char * ipaddr = inet_ntoa(in);
	sprintf(url, "http://reverseip.domaintools.com/search/?q=%s", ipaddr);
}

void mysql_initialize() {
	char *server = getenv("MYSQL_HOST");
	char *user = getenv("MYSQL_USER");
	char *password = getenv("MYSQL_PASS");
	char *database = getenv("MYSQL_DB");
	mysql_conn_select = mysql_init(NULL);
	mysql_conn_update = mysql_init(NULL);
	/* Connect to database */
	printf("Connecting to mysqldb...\n");
	if (mysql_real_connect(mysql_conn_select, server, user, password, database, 0, NULL, 0) == 0|| 
		mysql_real_connect(mysql_conn_update, server, user, password, database, 0, NULL, 0) == 0 ) {
		fprintf(stderr, "%s\n", mysql_error(mysql_conn_select));
		fprintf(stderr, "%s\n", mysql_error(mysql_conn_update));
		fprintf(stderr, "User %s Pass %s Host %s Database %s\n", user, password, server, database);
		exit(1);
	}
	printf("Connected!\n");
}


int main(char ** argv, int argc) {
	printf(
"  __      __  __      __  ______  ______  ______    \n"
" /\\ \\  __/\\ \\/\\ \\  __/\\ \\/\\__  _\\/\\__  _\\/\\__  _\\   \n"
" \\ \\ \\/\\ \\ \\ \\ \\ \\/\\ \\ \\ \\/_/\\ \\/\\/_/\\ \\/\\/_/\\ \\/   \n"
"  \\ \\ \\ \\ \\ \\ \\ \\ \\ \\ \\ \\ \\ \\ \\ \\   \\ \\ \\   \\ \\ \\   \n"
"   \\ \\ \\_/ \\_\\ \\ \\ \\_/ \\_\\ \\ \\_\\ \\__ \\ \\ \\   \\ \\ \\  \n"
"    \\ `\\___x___/\\ `\\___x___/ /\\_____\\ \\ \\_\\   \\ \\_\\ \n"
"     '\\/__//__/  '\\/__//__/  \\/_____/  \\/_/    \\/_/ \n"
"                                                    \n"
"         World Wide Internet Takeover Tool          \n"
"                Reverse IP crawler                  \n"  );

	curl_global_init(CURL_GLOBAL_DEFAULT);
	mysql_initialize();
	compile_regexp();
	
	// Create some CURL workers
	struct pqueue job_queue;
	pqueue_init(&job_queue);
	pthread_t curl_workers[NUM_WORKERS];
	int i;
	for (i = 0; i < NUM_WORKERS; i++)
		pthread_create (&curl_workers[i], NULL, &worker_thread, &job_queue);

	// Now query IPs and enqueue jobs
	mysql_query(mysql_conn_select, "SELECT `ip` FROM hosts WHERE reverseIpStatus=0");
	MYSQL_RES *result = mysql_store_result(mysql_conn_select);
	MYSQL_ROW row;
	while (result && (row = mysql_fetch_row(result))) {
		unsigned int ip = ntohl(atoi(row[0]));
		
		for (i = 0; i < 5; i++) {
			struct job_object * njob = malloc(sizeof(struct job_object));
			create_bing_url(njob->url,ip,i);
			njob->callback = bing_parser;
			
			pqueue_push(&job_queue, njob);
		}
		struct job_object * njob = malloc(sizeof(struct job_object));
		create_dt_url(njob->url,ip);
		njob->callback = dt_parser;
		pqueue_push(&job_queue, njob);

		while (pqueue_size(&job_queue) > NUM_WORKERS*3)
			sleep(1);

		printf("%d jobs in the queue\n", pqueue_size(&job_queue));
	}
	
	// Now stop all the threads
	pqueue_release(&job_queue);
	for (i = 0; i < NUM_WORKERS; i++)
		pthread_join (curl_workers[i],0);
	
	curl_global_cleanup();
} 


