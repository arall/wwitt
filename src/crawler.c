
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

#define MAX_OUTSTANDING_QUERIES (1024*64)
struct connection_query {
	int socket;
	unsigned int ip;
	unsigned short port;
	unsigned char status; // 0 unused, 1 connecting, 2 sending/receiving data
	unsigned char retries;
	
	void * usrdata;
	int tosend_max, tosend_offset;
	int received;
	char *inbuffer, *outbuffer;
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
	char tempbuf[8*1024];
	sprintf(tempbuf, "GET %s HTTP/1.1\r\nHost: %s\r\nUser-Agent: Mozilla/5.0 (Windows; U; Windows NT 5.1; rv:22.0) Gecko/20100101 Firefox/22.0\r\nConnection: close\r\n\r\n", path, vhost);
	int len = strlen(tempbuf);
	char * b = malloc(len+1);
	memcpy(b,tempbuf, len+1);
	
	return b;
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
	
	int bannercrawl = (strcmp("banner",argv[1]) == 0);
	
	// Initialize structures
	memset(&connection_table, 0, sizeof(connection_table));
	
	mysql_initialize();
	
	// Start!
	pthread_t db;
	pthread_create (&db, NULL, &database_dispatcher, &bannercrawl);
	
	// Infinite loop: query IP/Domain blocks
	char * query = "SELECT DISTINCT ip, host FROM virtualhosts WHERE head IS null OR index IS null OR robots IS null LIMIT %d";
	if (bannercrawl) query = "SELECT ip, port FROM services WHERE head IS null LIMIT %d";
	while (1) {
		// Generate queries and generate new connections
		char tquery[1024];
		sprintf(tquery, query, 100);
		mysql_query(mysql_conn_select, tquery);
		MYSQL_RES *result = mysql_store_result(mysql_conn_select);
		if (!result) break;
		
		MYSQL_ROW row;
		while ((row = mysql_fetch_row(result))) {
			struct connection_query cquery;
			cquery.ip = atoi(row[0]);
			cquery.status = 1;
			cquery.retries = 0;
			cquery.tosend_max = 0;
			cquery.tosend_offset = 0;
			cquery.received = 0;
			cquery.inbuffer = malloc(READBUF_CHUNK);
			memset(cquery.inbuffer,0,READBUF_CHUNK);
			cquery.outbuffer = 0;
			cquery.usrdata = 0;
			cquery.socket = socket(AF_INET, SOCK_STREAM, 0);
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
		
		// Connection loop
		int num_active = 0;
		struct connection_query * cq;
		for (cq = &connection_table[0]; cq != NULL; cq++) {
			if (cq->status == 1) {
				struct sockaddr_in sin;
				sin.sin_port = htons(cq->port);
				sin.sin_addr.s_addr = htons(cq->ip);
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
				if (rec > 0) {
					cq->received += rec;
					cq->inbuffer = realloc(cq->inbuffer, cq->received + READBUF_CHUNK);
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
			
			if (cq->status > 0 && cq->status < 32) {
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

// Walk the table from time to time and insert results into the database
void * database_dispatcher(void * args) {
	int bannercrawl = *(int*)args;
	char sql_query[64*1024];
	unsigned long long num_processed = 0;

	while (!adder_finish) {
		// Look for successful or failed transactions
		int i;
		for (i = 0; i < MAX_OUTSTANDING_QUERIES; i++) {
			struct connection_query * cquery = &connection_table[i];
			if (cquery->status == 100 || cquery->status == 101) {
				if (bannercrawl) {
					char tempb[cquery->received*2+2];
					mysql_real_escape_string(mysql_conn_update, tempb, cquery->inbuffer, cquery->received);
					sprintf(sql_query, "UPDATE from services SET head='%s' WHERE ip=%d AND port=%d\n",
						tempb, cquery->ip, cquery->port);
				}else{
					char tempb[cquery->received*2+2];
					mysql_real_escape_string(mysql_conn_update, tempb, cquery->inbuffer, cquery->received);
					char tempb2[strlen(cquery->usrdata)*2+2];
					mysql_real_escape_string(mysql_conn_update, tempb2, cquery->usrdata, strlen(cquery->usrdata));
					sprintf(sql_query, "UPDATE from virtualhosts SET head='%s' WHERE ip=%d AND host=%s\n",
						tempb, cquery->ip, tempb2);
				}
				num_processed++;
				mysql_query(mysql_conn_update,sql_query);
				cquery->status = 0; // Mark as unused
			}
		}
		sleep(1);
	}
	
	printf("End of crawl, quitting!\n");
	printf("Inserted %llu registers\n", num_processed);

	mysql_close(mysql_conn_update);
	exit(0);
}



