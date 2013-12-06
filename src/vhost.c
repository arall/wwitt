
#define _GNU_SOURCE 

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>
#include <pthread.h>
#include <mysql.h>
#include <arpa/inet.h>
#include <regex.h>
#include <pcre.h>
#include "pqueue.h"

int verbose = 1;

// Maximum query size, 1MB
#define MAX_BUFFER_SIZE   (1024*1024)
#define NUM_WORKERS       4

MYSQL *mysql_conn_select;
MYSQL *mysql_conn_update;
pthread_mutex_t update_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t ocr_mutex = PTHREAD_MUTEX_INITIALIZER;

struct job_object;
typedef void (*callback_fn)(void*,int,struct pqueue*,struct job_object *);

struct http_query {
	char * buffer;
	int received;
};
struct job_object {
	char url[8*1024];
	char url2[8*1024];
	char post[8*1024];
	unsigned int ip;
	unsigned int n;
	callback_fn callback;
};

struct job_object * new_job() {
	struct job_object * j = malloc(sizeof(struct job_object));
	memset(j,0,sizeof(struct job_object));
	return j;
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
void create_webhostinfo_url(char * url, unsigned int ip, int pagen) {
	struct in_addr in;
	in.s_addr = ip;
	char * ipaddr = inet_ntoa(in);
	sprintf(url, "http://whois.webhosting.info/%s?pi=%d", ipaddr, pagen);
}


static size_t curl_fwrite(void *buffer, size_t size, size_t nmemb, void *stream) {
	struct http_query * q = (struct http_query *)stream;

	q->buffer = realloc(q->buffer, size*nmemb + q->received + 32);
	char *bb = q->buffer;

	memcpy(&bb[q->received], buffer, size*nmemb);
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
		hq.buffer = malloc(32);

		CURL * curl = curl_easy_init();
		curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0);
		curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0);
		curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1);
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_fwrite);
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, &hq);
		curl_easy_setopt(curl, CURLOPT_URL, job->url);

		if (job->post[0] != 0) {
			curl_easy_setopt(curl, CURLOPT_POST, 1);
			curl_easy_setopt(curl, CURLOPT_POSTFIELDS, job->post);
		}

		if (verbose)
			printf("Fetch %s\n",job->url);
		CURLcode res = curl_easy_perform(curl);

		if(CURLE_OK == res) {
			// Call the callback with the data fetched
			job->callback(hq.buffer, hq.received, q, job);
		}

		curl_easy_cleanup(curl);
		
		free(job);
		job = pqueue_pop(q);
	}
	
	printf("Ending thread\n");
}

struct re_pattern_buffer reg1, reg2, reg3;
pcre * reg4, * reg5, * reg6, *reg7;
void compile_regexp() {
	const char *error;
    int   erroffset;

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

	reg3.translate = 0;
	reg3.fastmap = 0;
	reg3.buffer = 0;
	reg3.allocated = 0;
	re_set_syntax(RE_SYNTAX_POSIX_EXTENDED);
	const char * pat3 = "<span[^>]*title=\"[0-9]*\\.[0-9]*\\.[0-9]*\\.[0-9]*\"[^>]*>([^<]*)</span>";
	re_compile_pattern(pat3, strlen(pat3), &reg3); 

	const char * pat4 = "<td><a[^>]*href=\"http://whois.webhosting.info/[^>\"]*\"[^>]*>([^<]*)\\.</a></td>";
	reg4 = pcre_compile (pat4, PCRE_MULTILINE, &error, &erroffset, 0);

	const char * pat5 = "(http://charting.webhosting.info/scripts/sec.php\\?ec=[a-zA-Z0-9\\-]*)";
	reg5 = pcre_compile (pat5, PCRE_MULTILINE, &error, &erroffset, 0);

	const char * pat6 = "<input[^>]*type='hidden'[^>]*name='enck'[^>]* value='([^']*)'>";
	reg6 = pcre_compile (pat6, PCRE_MULTILINE, &error, &erroffset, 0);
}

void database_insert(const char * host, const char * ipaddr) {
	char tquery[8*1024];
	struct in_addr in;
	inet_aton(ipaddr,&in); unsigned int ip = ntohl(in.s_addr);

	pthread_mutex_lock(&update_mutex);
	if (host) {
		sprintf(tquery, "INSERT INTO `virtualhosts` (`ip`, `host`, `dateAdd`) VALUES (%d, '%s', now())", ip, host);
		mysql_query(mysql_conn_update, tquery);
	}
	else
	{
		sprintf(tquery, "UPDATE `hosts` SET reverseIpStatus=1 WHERE ip=%d", ip);
		mysql_query(mysql_conn_update, tquery);
		sprintf(tquery, "INSERT INTO `virtualhosts` (`ip`, `host`, `dateAdd`) VALUES (%d, '%s', now())", ip, ipaddr);
		mysql_query(mysql_conn_update, tquery);
	}
	pthread_mutex_unlock(&update_mutex);
}

char * decode1(void * buffer, int size);
void webhostinfo_parser(void * buffer, int size, struct pqueue * job_queue,struct job_object *job);

void webhostinfo_captcha(void * buffer, int size, struct pqueue * job_queue, struct job_object *job) {
	if (verbose)
		printf("Received captcha\n");

	// Send the captcha to the captach-solving engine :D sounds cool
	pthread_mutex_lock(&ocr_mutex);
	char * code = decode1(buffer, size);
	pthread_mutex_unlock(&ocr_mutex);

	char * srch_value = job->url2 + strlen(job->url2)-1;
	while (*srch_value != '/')
		srch_value--;
	srch_value++;

	char * enck = job->url + strlen(job->url)-1;
	while (*enck != '=')
		enck--;
	enck++;

	struct job_object * njob = new_job();
	memset(njob->url,0,sizeof(njob->url));
	memcpy(njob->url,job->url2,strlen(job->url2));
	njob->callback = webhostinfo_parser;
	njob->ip = job->ip;
	njob->n = job->n;
	sprintf(njob->post, "enck=%s&srch_value=%s&code=%s&subSecurity=Submit", enck, srch_value, code);
	pqueue_push_front(job_queue, njob);

	if (verbose)
		printf("Solved captch, Code %s\n",code);
	free(code);
}

// Parse BING result page
void bing_parser(void * buffer, int size, struct pqueue * job_queue,struct job_object *job) {
	char * cbuffer = (char*)buffer;
	cbuffer[size] = 0; // We have some space after the buffer

	char ipaddr[1024]; memset(ipaddr,0,sizeof(ipaddr));
	struct re_registers regs;
	if (re_search(&reg2,cbuffer,size,0,size,&regs)!=-1) {
		if (regs.start[1] >= 0) {
			memcpy(ipaddr,&cbuffer[regs.start[1]],regs.end[1]-regs.start[1]);
			database_insert(0, ipaddr);
		}
	}

	int off = 0;
	while (re_search(&reg1,cbuffer,size,off,size,&regs) != -1) {
		if (regs.start[1] >= 0) {
			char temp[4096]; memset(temp,0,sizeof(temp));
			memcpy(temp,&cbuffer[regs.start[1]],regs.end[1]-regs.start[1]);
			char * slash = strstr(temp,"/");
			if (slash) *slash = 0;
			database_insert(temp, ipaddr);
			//printf("Match found %s %s\n",temp,ipaddr);
			off = regs.end[1];
		}
		else break;
	}
}
// Parse DT result page
void dt_parser(void * buffer, int size, struct pqueue * job_queue,struct job_object *job) {
	char * cbuffer = (char*)buffer;
	cbuffer[size] = 0; // We have some space after the buffer

	char ipaddr[1024]; memset(ipaddr,0,sizeof(ipaddr));
	struct re_registers regs;
	int off = 0;
	while (re_search(&reg3,cbuffer,size,off,size,&regs) != -1) {
		if (regs.start[1] >= 0) {
			char temp[4096]; memset(temp,0,sizeof(temp));
			memcpy(temp,&cbuffer[regs.start[1]],regs.end[1]-regs.start[1]);
			char * slash = strstr(temp,"/");
			if (slash) *slash = 0;
			//database_insert(temp, ipaddr);
			printf("Match found %s %s\n",temp,ipaddr);
			off = regs.end[1];
		}
		else break;
	}
}
// Parse WebHostingInfo result page
void webhostinfo_parser(void * buffer, int size, struct pqueue * job_queue,struct job_object *job) {
	char * cbuffer = (char*)buffer;
	cbuffer[size] = 0; // We have some space after the buffer

	int ovector[9];

	struct in_addr in;
	in.s_addr = job->ip;
	char * ipaddr = inet_ntoa(in);

	char img_url[4096]; memset(img_url,0,sizeof(img_url));
	if (pcre_exec(reg5, 0, cbuffer, size, 0, 0, ovector, 9) > 0) {
		memcpy(img_url,&cbuffer[ovector[2]],ovector[3]-ovector[2]);
		if (verbose)
			printf("Found captcha request on image %s\n", img_url);
		// Submit a new job to download the cpatcha and be able to post the info
		// We save the original Url as Url2 to resubmit the job :D
		struct job_object * njob = new_job();
		memset(njob->url,0,sizeof(njob->url));
		memcpy(njob->url,img_url,strlen(img_url));
		memcpy(njob->url2,job->url,sizeof(njob->url2));
		njob->ip = job->ip;
		njob->n = job->n;
		njob->callback = webhostinfo_captcha;
		pqueue_push_front(job_queue, njob);
		return;
    }

	int off = 0;
	while (pcre_exec(reg4, 0, cbuffer, size, off, 0, ovector, 9) > 0) {
		char temp[4096]; memset(temp,0,sizeof(temp));
		memcpy(temp,&cbuffer[ovector[2]],ovector[3]-ovector[2]);
		database_insert(temp, ipaddr);
		if (verbose)
			printf("Match found %s\n",temp);
		off = ovector[1];
	}

	// Schedule a new page if there's any
	if (strstr(cbuffer,"Next&nbsp;&gt;&gt;") != 0) {
		struct job_object * njob = new_job();
		njob->ip = job->ip;
		njob->n = job->n+1;
		create_webhostinfo_url(njob->url,njob->ip,njob->n);
		njob->callback = webhostinfo_parser;
		pqueue_push_front(job_queue, njob);
	}
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
	init_captcha();
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
		
		//create_bing_url(njob->url,ip,i);
		//njob->callback = bing_parser;
		//pqueue_push(&job_queue, njob);
		//njob = new_job();

		struct job_object * njob = new_job();
		njob->ip = ip;
		njob->n = 1;
		create_webhostinfo_url(njob->url,ip,1);
		njob->callback = webhostinfo_parser;
		pqueue_push(&job_queue, njob);

		//struct job_object * njob = new_job();
		//create_dt_url(njob->url,ip);
		//njob->callback = dt_parser;
		//pqueue_push(&job_queue, njob);

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


