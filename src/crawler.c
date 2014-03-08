
#define _GNU_SOURCE

#include <poll.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <signal.h>
#include <pthread.h>
#include <semaphore.h>
#include <mysql.h>
#include <sys/resource.h> 

#define BUFSIZE (1024*1024)   // maximum size of any datagram(16 bits the size of identifier)
#define TRUE 1
#define FALSE 0
#define default_low  1
#define default_high 1024

#define READBUF_CHUNK 4096

#define MAX_TIMEOUT 10000   // In mseconds
#define MAX_RETRIES 2

#define MAX_OUTSTANDING_QUERIES (1024*64)
struct connection_query {
	int socket;
	unsigned int ip;
	unsigned short port;
	unsigned char status; // 0 unused, 1 connecting, 2 sending/receiving data
	unsigned char retries;
	unsigned char redirs;
	
	void * usrdata;
	int tosend_max, tosend_offset;
	int received;
	char *inbuffer, *outbuffer;
	unsigned long start_time;
} connection_table[MAX_OUTSTANDING_QUERIES];

struct pollfd poll_desc[MAX_OUTSTANDING_QUERIES];

MYSQL *mysql_conn_select;
MYSQL *mysql_conn_update;

void * database_dispatcher(void * args);
void * query_adder(void * args);
void mysql_initialize();

#define CONNECT_ERR(ret) (ret < 0 && errno != EINPROGRESS && errno != EALREADY && errno != EISCONN)
#define CONNECT_OK(ret) (ret >= 0 || (ret < 0 && errno == EISCONN))
#define IOTRY_AGAIN(ret) (ret < 0 && (errno == EAGAIN || errno == EWOULDBLOCK))

int sockfd;
struct in_addr current_ip;

int maxpp;
int total_ips, total_ports;
int portlist[32];
volatile int adder_finish = 0;

int setNonblocking(int fd) {
	int flags;
	/* If they have O_NONBLOCK, use the Posix way to do it */
#if defined(O_NONBLOCK)
	/* Fixme: O_NONBLOCK is defined but broken on SunOS 4.1.x and AIX 3.2.5. */
	if (-1 == (flags = fcntl(fd, F_GETFL, 0)))
		flags = 0;
	return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
#else
	/* Otherwise, use the old way of doing it */
	flags = 1;
	return ioctl(fd, FIOBIO, &flags);
#endif
}

char * generateHTTPQuery(const char * vhost, const char * path) {
	if (strlen(path) == 0) path = "/";
	char tempbuf[8*1024];
	sprintf(tempbuf, "GET %s HTTP/1.1\r\nHost: %s\r\nUser-Agent: Mozilla/5.0 (Windows; U; Windows NT 5.1; rv:22.0) Gecko/20100101 Firefox/22.0\r\nConnection: close\r\n\r\n", path, vhost);
	int len = strlen(tempbuf);
	char * b = malloc(len+1);
	memcpy(b,tempbuf, len+1);
	
	return b;
}

char* parse_response(char * buffer, int size, const char * domain) {
	// Look for redirects only in the header
	buffer[size] = 0;
	char * limit = strstr(buffer,"\r\n\r\n");
	char * location = strcasestr(buffer,"Location:");

	if (limit == 0 || location == 0) return 0;
	if ((uintptr_t)location > (uintptr_t)limit) return 0;

	// We found a Location in the headers!
	location += 9;
	while (*location == ' ') location++;

	int i;
	char temp[4096] = {0};
	for (i = 0; i < 4096 && *location != '\r'; i++)
		temp[i] = *location++;

	// Append a proper header in case it's just a relative URL
	char * ret = calloc(4096,1);
	if (memcmp(temp,"http:",5) == 0 || memcmp(temp,"https:",6) == 0) {
		sprintf(ret,"%s",&temp[7]);
		for (i = 0; i < strlen(ret); i++)
			if (ret[i] == '/')
				ret[i] = 0;
	}
	else {
		// Get the Base URL
		sprintf(ret,"%s\0%s",domain,temp);
	}
	return ret;
}

void clean_entry(struct connection_query * q) {
	close(q->socket);
	if (q->usrdata) free(q->usrdata);
	if (q->inbuffer) free(q->inbuffer);
	if (q->outbuffer) free(q->outbuffer);
	q->status = 0;
}

int main(int argc, char **argv) {
	char errbuf[1024];
	
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

	
	if ( argc < 2 || (strcmp("banner",argv[1]) == 0 && argc < 5) ) {
		fprintf(stderr, "Usage: %s http\n", argv[0]);
		fprintf(stderr, "Usage: %s banner IPstart IPend ports{max 32}\n", argv[0]);
		exit(1);
	}
	
	struct rlimit limit; limit.rlim_cur = MAX_OUTSTANDING_QUERIES*1.1f; limit.rlim_max = MAX_OUTSTANDING_QUERIES*1.6f;
	if (setrlimit(RLIMIT_NOFILE, &limit)<0) {
		fprintf(stderr, "setrlimit failed!\n");
		exit(1);
	}
	
	int max_inflight = 100;
	int bannercrawl = (strcmp("banner",argv[1]) == 0);
	
	// Initialize structures
	memset(&connection_table, 0, sizeof(connection_table));
	
	mysql_initialize();
	
	// Start!
	pthread_t db;
	pthread_create (&db, NULL, &database_dispatcher, &bannercrawl);

	int num_active = 0;	
	// Infinite loop: query IP/Domain blocks
	char * query = "SELECT DISTINCT `ip`, `host` FROM virtualhosts WHERE `head` IS null OR `index` IS null  LIMIT %d";
	if (bannercrawl) query = "SELECT `ip`, `port` FROM services WHERE `head` IS null LIMIT %d";
	while (1) {
		// Generate queries and generate new connections
		if (num_active < max_inflight) {
			printf("Num active %d\n",num_active);
			char tquery[1024];
			sprintf(tquery, query, 100);
			mysql_query(mysql_conn_select, tquery);
			MYSQL_RES *result = mysql_store_result(mysql_conn_select);
			if (!result) break;
		
			MYSQL_ROW row;
			while ((row = mysql_fetch_row(result))) {
				struct connection_query cquery;
				cquery.ip = (atoi(row[0]));
				cquery.status = 1;
				cquery.retries = 0;
				cquery.redirs = 0;
				cquery.tosend_max = 0;
				cquery.tosend_offset = 0;
				cquery.received = 0;
				cquery.inbuffer = malloc(READBUF_CHUNK+32);
				memset(cquery.inbuffer,0,READBUF_CHUNK+32);
				cquery.outbuffer = 0;
				cquery.usrdata = 0;
				cquery.socket = socket(AF_INET, SOCK_STREAM, 0);
				cquery.start_time = time(0);
				setNonblocking(cquery.socket);
			
				if (bannercrawl) {
					cquery.port = atoi(row[1]);
				}else{
					char * vhost = row[1];
					cquery.outbuffer = generateHTTPQuery(vhost,"/");
					cquery.tosend_max = strlen(cquery.outbuffer);
					cquery.port = 80;
					cquery.usrdata = malloc(strlen(vhost)+1);
					memcpy(cquery.usrdata,vhost,strlen(vhost)+1);
				}
			
				// Look for an empty entry
				int i;
				for (i = 0; i < MAX_OUTSTANDING_QUERIES; i++) {
					if (connection_table[i].status == 0) {
						memcpy(&connection_table[i], &cquery, sizeof(cquery));
						break;
					}
				}
			}
		}
		
		// Connection loop
		num_active = 0;
		struct connection_query * cq;
		for (cq = &connection_table[0]; cq != &connection_table[MAX_OUTSTANDING_QUERIES]; cq++) {
			if (cq->status == 1) {
				struct sockaddr_in sin;
				sin.sin_port = htons(cq->port);
				sin.sin_addr.s_addr = htonl(cq->ip);
				sin.sin_family = AF_INET;
			
				int res = connect(cq->socket,(struct sockaddr *)&sin,sizeof(sin));
				if (CONNECT_ERR(res)) {
					// Drop connection!
					cq->status = 99;
				}
				else if (CONNECT_OK(res)) {
					cq->status = 2;
				}
			}
			if (cq->status == 2) {
				// Try to send data (if any)
				if (cq->tosend_offset < cq->tosend_max) {
					int sent = write(cq->socket,&cq->outbuffer[cq->tosend_offset],cq->tosend_max-cq->tosend_offset);
					if (sent > 0) {
						cq->tosend_offset += sent;
					}
					else if (!IOTRY_AGAIN(sent)) {
						cq->status = 99;
					}
				}
			
				// Try to receive data
				int rec = read(cq->socket, &cq->inbuffer[cq->received], READBUF_CHUNK);
				if (cq->received > BUFSIZE) rec = 0; // Force stream end at BUFSIZE
				if (rec > 0) {
					cq->received += rec;
					cq->inbuffer = realloc(cq->inbuffer, cq->received + READBUF_CHUNK + 32);
					cq->inbuffer[cq->received] = 0; // Add zero to avoid strings running out the buffer
				}
				else if (rec == 0) {
					// End of data. OK if we sent all the data only
					if (cq->tosend_offset < cq->tosend_max) 
						cq->status = 99; // Disconnect, but ERR!
					else
						cq->status = 100; // Disconnect, OK!
				}
				else if (!IOTRY_AGAIN(rec)) {
					// Connection error!
					cq->status = 99;
				}
			}
			
			if (cq->status == 99) {
				if (cq->retries++ < MAX_RETRIES) {
					cq->status = 1;
					cq->tosend_offset = 0;
					cq->received = 0;
					cq->start_time = time(0);
				}
				else {
					// We are done with this guy, mark is as false ready
					//clean_entry(cq);
					cq->received = 0;
					cq->status = 101;
				}
			}
			if (cq->status > 0 && cq->status < 32) {
				if (time(0) - cq->start_time > MAX_TIMEOUT)
					cq->status = 99;

				int mask = POLLERR;
				mask |= (cq->tosend_offset < cq->tosend_max) ? POLLOUT : 0;
				mask |= POLLIN;

				poll_desc[num_active].fd = cq->socket;
				poll_desc[num_active].events = mask;
				poll_desc[num_active].revents = 0;
				num_active++;
			}
		}
		
		// Waiting. Poll for sockets
		poll(poll_desc, num_active, 500);
	}
	
	printf("No more things to crawl, waiting for threads...\n");
	adder_finish = 1;
	mysql_close(mysql_conn_select);
	
	pthread_join(db, NULL);
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

void separate_body(char * buffer, int size, char ** buf1, char ** buf2, int * len1, int * len2) {
	buffer[size] = 0;
	char * bodybody = strstr(buffer,"\r\n\r\n");
	if (!bodybody) bodybody = &buffer[size];

	*len1 = ((uintptr_t)bodybody - (uintptr_t)buffer);
	*len2 = size - (*len1) - 4;
	if (*len2 < 0) *len2 = 0;

	bodybody += 4;
	*buf1 = buffer;
	*buf2 = bodybody;
}

int parse_hex(const char * cptr) {
	int ret = 0;
	int valid;
	do {
		valid = 1;
		
		if (*cptr >= '0' && *cptr <= '9')
			ret = (ret << 4) | (*cptr - '0');
		else if (*cptr >= 'a' && *cptr <= 'f')
			ret = (ret << 4) | (*cptr - 'a' + 10);
		else if (*cptr >= 'A' && *cptr <= 'F')
			ret = (ret << 4) | (*cptr - 'A' + 10);
		else
			valid = 0;
		cptr++;

	} while (valid);
	return ret;
}

// In case of "Transfer-Encoding: chunked" de-chunk the HTTP body
void dechunk_http(char * buffer, int * size) {
	char * head, * body;
	int head_len, body_len;
	separate_body(buffer, *size, &head, &body, &head_len, &body_len);
	if (body_len == 0) return;
	
	// Look for Transfer-encoding
	char * chunk = strcasestr(head, "Transfer-encoding");
	if (!chunk || ((uintptr_t)chunk > (uintptr_t)body) ) return;
	// Look for the "chunked" word before the next break line
	char * chunkend = strstr(chunk, "\r\n");
	if (!chunkend || ((uintptr_t)chunkend > (uintptr_t)body) ) return;
	
	char * chunked = strcasestr(chunk, "chunked");
	if (!chunked || ((uintptr_t)chunked > (uintptr_t)body) ) return;
	
	// Now proceed to dechunk the body
	char * newbuffer = malloc(*size+32);
	memcpy(newbuffer, buffer, head_len+4);
	char * newbody = &newbuffer[head_len+4];
	int newlen = head_len+4;
	while ( (uintptr_t)body < (uintptr_t)&buffer[*size] ) {
		int len = parse_hex(body);
		if (len + newlen > *size) break; // Overflow, should not happen
		if (len == 0) break;  // Last chunk
		body = strstr(body, "\r\n");
		if (!body) break;
		body += 2;
		memcpy(newbody, body, len);
		newbody += len;
		body += (len+2);  // Skip \r\n
		newlen += len;
	}
	
	// Copy and free!
	memcpy(buffer, newbuffer, newlen);
	*size = newlen;
	free(newbuffer);
}

// Walk the table from time to time and insert results into the database
void * database_dispatcher(void * args) {
	int bannercrawl = *(int*)args;
	char sql_query[BUFSIZE*2];
	unsigned long long num_processed = 0;

	while (!adder_finish) {
		// Look for successful or failed transactions
		int i;
		for (i = 0; i < MAX_OUTSTANDING_QUERIES; i++) {
			struct connection_query * cquery = &connection_table[i];
			if (cquery->status == 100 || cquery->status == 101) {
				// Analyze the response, maybe we find some redirects
				dechunk_http(cquery->inbuffer,&cquery->received);
				char* newloc = parse_response(cquery->inbuffer, cquery->received, cquery->usrdata);
				
				if (newloc != 0) {
					// Reuse the same entry for the request
					//printf("%s\n", cquery->usrdata);
					cquery->retries = 0;
					cquery->redirs++;
					cquery->tosend_offset = 0;
					cquery->received = 0;
					cquery->inbuffer = realloc(cquery->inbuffer,READBUF_CHUNK+32);
					memset(cquery->inbuffer,0,READBUF_CHUNK+32);
					close(cquery->socket);
					cquery->socket = socket(AF_INET, SOCK_STREAM, 0);
					cquery->start_time = time(0);
					setNonblocking(cquery->socket);

					//free(cquery->usrdata);
					free(cquery->outbuffer);
					char * path = newloc + (strlen(newloc)+1);
					cquery->outbuffer = generateHTTPQuery(newloc,path);
					cquery->tosend_max = strlen(cquery->outbuffer);
					//cquery->usrdata = newloc;
					free(newloc);

					//printf("Redir! %s %s\n", newloc, cquery->outbuffer);
					cquery->status = 1;
				}
				else{ 
					if (bannercrawl) {
						char tempb[cquery->received*2+2];
						mysql_real_escape_string(mysql_conn_update, tempb, cquery->inbuffer, cquery->received);
						sprintf(sql_query, "UPDATE services SET `head`='%s' WHERE `ip`=%d AND `port`=%d\n",
							tempb, cquery->ip, cquery->port);
					}else{
						char tempb [cquery->received*2+2];
						char tempb_[cquery->received*2+2];
						int len1, len2; char *p1, *p2;
						separate_body(cquery->inbuffer, cquery->received, &p1, &p2, &len1, &len2);

						mysql_real_escape_string(mysql_conn_update, tempb,  p1, len1);
						mysql_real_escape_string(mysql_conn_update, tempb_, p2, len2);
						char tempb2[strlen(cquery->usrdata)*2+2];
						mysql_real_escape_string(mysql_conn_update, tempb2, cquery->usrdata, strlen(cquery->usrdata));
						sprintf(sql_query, "UPDATE virtualhosts SET `index`='%s',`head`='%s' WHERE `ip`=%d AND `host`=\"%s\"\n", tempb_, tempb, cquery->ip, tempb2);
					}
					num_processed++;
					mysql_query(mysql_conn_update,sql_query);

					// Entry cleanup
					clean_entry(cquery);
				}
			}
		}
		sleep(1);
	}
	
	printf("End of crawl, quitting!\n");
	printf("Inserted %llu registers\n", num_processed);

	mysql_close(mysql_conn_update);
	exit(0);
}



