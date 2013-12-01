
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>
#include <pthread.h>
#include <mysql.h>
#include <arpa/inet.h>
#include "pqueue.h"

// Maximum query size, 1MB
#define MAX_BUFFER_SIZE   (1024*1024)
#define NUM_WORKERS       16

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

	if (q->buffer == NULL) q->buffer = malloc(size*nmemb);
	else q->buffer = realloc(q->buffer, size*nmemb + q->received);

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

// Parse BING result page
void bing_parser(void * buffer, int size) {
	
}

void create_bing_url(char * url, unsigned int ip, int pagen) {
	struct in_addr in;
	in.s_addr = ip;
	char * ipaddr = inet_ntoa(in);
	sprintf(url, "http://www.bing.com/search?first=%d&q=IP:%20%s", pagen, ipaddr);
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
	if (!mysql_real_connect(mysql_conn_select, server, user, password, database, 0, NULL, 0) || 
		!mysql_real_connect(mysql_conn_update, server, user, password, database, 0, NULL, 0) ) {
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
"                Web/Banner crawler                  \n"  );

	curl_global_init(CURL_GLOBAL_DEFAULT);
	mysql_initialize();
	
	// Create some CURL workers
	struct pqueue job_queue;
	pqueue_init(&job_queue);
	pthread_t curl_workers[NUM_WORKERS];
	int i;
	for (i = 0; i < NUM_WORKERS; i++)
		pthread_create (&curl_workers[i], NULL, &worker_thread, &job_queue);

	// Now query IPs and enqueue jobs
	mysql_query(mysql_conn_select, "SELECT ip FROM ...");
	MYSQL_RES *result = mysql_store_result(mysql_conn_select);
	MYSQL_ROW row;
	while (result && (row = mysql_fetch_row(result))) {
		unsigned int ip = atoi(row[0]);
		
		for (i = 0; i < 5; i++) {
			struct job_object * njob = malloc(sizeof(struct job_object));
			create_bing_url(njob->url,ip,i);
			njob->callback = bing_parser;
			
			pqueue_push(&job_queue, njob);
		}

		while (pqueue_size(&job_queue) > NUM_WORKERS*10)
			sleep(1);
	}
	
	// Now stop all the threads
	pqueue_release(&job_queue);
	for (i = 0; i < NUM_WORKERS; i++)
		pthread_join (curl_workers[i],0);
	
	curl_global_cleanup();
} 


