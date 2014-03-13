
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <curl/curl.h>
#include <iostream>
#include <poll.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
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
#include <errmsg.h>
#include <sys/resource.h> 

#define BUFSIZE (1024*1024)   // maximum size of any datagram(16 bits the size of identifier)
#define TRUE 1
#define FALSE 0
#define default_low  1
#define default_high 1024

#define READBUF_CHUNK 4096

#define MAX_TIMEOUT 10   // In seconds
#define MAX_RETRIES 10
#define MAX_REDIRS  10

enum RequestsStatus {
	reqEmpty,
	reqDnsQuery, reqDnsQuerying, 
	reqConnecting, reqTransfer, 
	reqCurl, reqCurlTransfer,
	reqComplete, reqCompleteError, reqError
};

#define MAX_OUTSTANDING_QUERIES (1024*256)
struct connection_query {
	int socket;
	unsigned long ip;
	unsigned short port;
	RequestsStatus status;
	unsigned char retries;
	unsigned char redirs;
	
	std::string vhost;
	std::string url;
	int tosend_max, tosend_offset;
	int received;
	char *inbuffer, *outbuffer;
	unsigned long start_time;
} connection_table[MAX_OUTSTANDING_QUERIES];

struct pollfd poll_desc[MAX_OUTSTANDING_QUERIES];

MYSQL *mysql_conn_select = 0;
MYSQL *mysql_conn_update = 0;
MYSQL *mysql_conn_update2 = 0;

void * database_dispatcher(void * args);
void * dns_dispatcher(void * args);
void * curl_dispatcher(void * args);
void mysql_initialize();

#define CONNECT_ERR(ret) (ret < 0 && errno != EINPROGRESS && errno != EALREADY && errno != EISCONN)
#define CONNECT_OK(ret) (ret >= 0 || (ret < 0 && (errno == EISCONN || errno == EALREADY)))
#define IOTRY_AGAIN(ret) (ret < 0 && (errno == EAGAIN || errno == EWOULDBLOCK))

volatile int adder_finish = 0;

int max_inflight = 100;
int max_dns_inflight = 50;
int curl_workers = 20;

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

std::string mysql_real_escape_std_string(MYSQL * c, const std::string s) {
	char tempb[s.size()*2+2];
	mysql_real_escape_string(c, tempb, s.c_str(), s.size());
	return std::string(tempb);
}

std::string generateHTTPQuery(const std::string vhost, const std::string path) {
	std::string p = (path == "") ? "/" : path;

	return "GET " + p + " HTTP/1.1\r\nHost: " + vhost +  \
		"\r\nUser-Agent: Mozilla/5.0 (Windows; U; Windows NT 5.1; rv:22.0) Gecko/20100101 Firefox/22.0\r\nConnection: close\r\n\r\n";
}

// Returns http[s]://domain.com/   given any URL under that website (base URL as its name indicates)
std::string base_url (const std::string & burl) {
	int s = 0;
	if (burl.substr(0,7) == "http://") s = 7;
	if (burl.substr(0,8) == "https://") s = 8;
	while (s < burl.size() && burl[s] != '/')
		s++;
	
	if (s == burl.size()) return burl + "/";
	return burl.substr(0,s+1);
}

std::string gethostname(const std::string & url) {
	std::string r = base_url(url);
	if (r.substr(0,7) == "http://")  r = r.substr(7);
	if (r.substr(0,8) == "https://") r = r.substr(8);
	
	size_t p = r.find("/");
	if (p == std::string::npos)
		return r;
	else
		return r.substr(0,p);
}
std::string getpathname(const std::string & url) {
	std::string r = base_url(url);
	if (r.substr(0,7) == "http://")  r = r.substr(7);
	if (r.substr(0,8) == "https://") r = r.substr(8);
	
	size_t p = r.find("/");
	if (p == std::string::npos)
		return "/";
	else
		return r.substr(p);
}

std::string parse_response(char * buffer, int size, const std::string current_url) {
	// Look for redirects only in the header
	buffer[size] = 0;
	char * limit = strstr(buffer,"\r\n\r\n");
	char * location = strcasestr(buffer,"Location:");

	if (limit == 0 || location == 0) return "";
	if ((uintptr_t)location > (uintptr_t)limit) return "";

	// We found a Location in the headers!
	location += 9;
	while (*location == ' ') location++;

	int i;
	std::string temploc;
	while (*location != '\r' && *location != ' ' && (uintptr_t)location < (uintptr_t)limit)
		temploc += *location++;

	// Append a proper header in case it's just a relative URL
	if (temploc.substr(0,7) == "http://" || temploc.substr(0,8) == "https://") {
		return temploc;
	}
	else {
		return base_url(current_url) + temploc;
	}
}

void clean_entry(struct connection_query * q) {
	close(q->socket);
	q->vhost = "";
	q->url = "";
	if (q->inbuffer) free(q->inbuffer);
	if (q->outbuffer) free(q->outbuffer);
	q->status = reqEmpty;
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


	fprintf(stderr, "Using about %u memory mega-bytes table\n", sizeof(connection_table)>>20);
	
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
	//memset(&connection_table, 0, sizeof(connection_table));
	for (int i = 0; i < sizeof(connection_table)/sizeof(connection_table[0]); i++)
		clean_entry(&connection_table[i]);
	
	signal(SIGPIPE, SIG_IGN);
	mysql_initialize();
	
	// Clear "inflight flag"
	mysql_query(mysql_conn_update,"UPDATE `virtualhosts` SET `status`=`status`&(~1)");
	
	// Start!
	pthread_t db;
	pthread_create (&db, NULL, &database_dispatcher, &bannercrawl);

	pthread_t dns_workers[max_dns_inflight];
	for (int i = 0; i < max_dns_inflight; i++)
		pthread_create (&dns_workers[i], NULL, &dns_dispatcher, (void*)i);

	int num_active = 0;	
	// Infinite loop: query IP/Domain blocks
	const char * query = "SELECT DISTINCT `host` FROM virtualhosts WHERE (`head` IS null OR `index` IS null) AND `status`&1 = 0";
	if (bannercrawl) query = "SELECT `ip`, `port` FROM services WHERE `head` IS null AND `status`&1 = 0";
	while (1) {
		// Generate queries and generate new connections
		if (num_active < max_inflight) {
			mysql_query(mysql_conn_select, query);
			MYSQL_RES *result = mysql_store_result(mysql_conn_select);
			if (!result) break;
			
			int num_rem = mysql_num_rows(result);
			if (num_rem == 0 && num_active == 0) break; // End!
			
			printf("\rNum active: %d Remaining: %d ...   ",num_active, num_rem); fflush(stdout);

			int injected = 0;		
			MYSQL_ROW row;
			while ((row = mysql_fetch_row(result)) && injected++ < max_inflight) {
				struct connection_query cquery;
				cquery.ip = 0;
				cquery.retries = cquery.redirs = 0;
				cquery.tosend_max = cquery.tosend_offset = 0;
				cquery.received = 0;
				cquery.inbuffer = (char*)malloc(READBUF_CHUNK+32);
				memset(cquery.inbuffer,0,READBUF_CHUNK+32);
				cquery.outbuffer = 0;
				cquery.vhost = "";
				cquery.url = "";
				cquery.socket = socket(AF_INET, SOCK_STREAM, 0);
				cquery.start_time = time(0);
				setNonblocking(cquery.socket);
			
				if (bannercrawl) {
					cquery.ip = atoi(row[0]);
					cquery.port = atoi(row[1]);
					cquery.status = reqConnecting;
				}else{
					std::string vhost = std::string(row[0]);
					cquery.outbuffer = strdup(generateHTTPQuery(vhost,"/").c_str());
					cquery.tosend_max = strlen(cquery.outbuffer);
					cquery.port = 80;
					cquery.status = reqDnsQuery;
					cquery.vhost = vhost;
					cquery.url = "http://" + vhost + "/";
					std::cout << "Query " << cquery.url << std::endl;
				}
			
				// Look for an empty entry
				int i;
				for (i = 0; i < MAX_OUTSTANDING_QUERIES; i++) {
					if (connection_table[i].status == reqEmpty) {
						connection_table[i] = cquery;
						break;
					}
				}
				
				// Mark it as in process
				if (!bannercrawl) {
					char upq[2048];
					sprintf(upq,"UPDATE `virtualhosts` SET `status`=`status`|1 WHERE `host`=\"%s\"", row[0]);
					mysql_query(mysql_conn_update2,upq);
				}
			}
		}
		
		// Connection loop
		num_active = 0;
		struct connection_query * cq;
		for (cq = &connection_table[0]; cq != &connection_table[MAX_OUTSTANDING_QUERIES]; cq++) {
			if (cq->status == reqConnecting) {
				struct sockaddr_in sin;
				sin.sin_port = htons(cq->port);
				sin.sin_addr.s_addr = htonl(cq->ip);
				sin.sin_family = AF_INET;
			
				int res = connect(cq->socket,(struct sockaddr *)&sin,sizeof(sin));
				if (CONNECT_ERR(res)) {
					// Drop connection!
					cq->status = reqError;
				}
				else if (CONNECT_OK(res)) {
					cq->status = reqTransfer;
				}
			}
			if (cq->status == reqTransfer) {
				// Try to send data (if any)
				if (cq->tosend_offset < cq->tosend_max) {
					int sent = write(cq->socket,&cq->outbuffer[cq->tosend_offset],cq->tosend_max-cq->tosend_offset);
					if (sent > 0) {
						cq->tosend_offset += sent;
					}
					else if (!IOTRY_AGAIN(sent)) {
						cq->status = reqError;
					}
				}
			
				// Try to receive data
				int rec = read(cq->socket, &cq->inbuffer[cq->received], READBUF_CHUNK);
				if (cq->received > BUFSIZE) rec = 0; // Force stream end at BUFSIZE
				if (rec > 0) {
					cq->received += rec;
					cq->inbuffer = (char*)realloc(cq->inbuffer, cq->received + READBUF_CHUNK + 32);
					cq->inbuffer[cq->received] = 0; // Add zero to avoid strings running out the buffer
				}
				else if (rec == 0) {
					// End of data. OK if we sent all the data only
					if (cq->tosend_offset < cq->tosend_max) 
						cq->status = reqError; // Disconnect, but ERR!
					else
						cq->status = reqComplete; // Disconnect, OK!
				}
				else if (!IOTRY_AGAIN(rec)) {
					// Connection error!
					cq->status = reqError;
				}
			}
			
			if (cq->status == reqError) {
				if (cq->retries++ < MAX_RETRIES) {
					cq->status = reqDnsQuery;
					cq->tosend_offset = 0;
					cq->received = 0;
					cq->start_time = time(0);
				}
				else {
					// We are done with this guy, mark is as false ready
					//clean_entry(cq);
					cq->received = 0;
					cq->status = reqCompleteError;
				}
			}
			if (cq->status == reqConnecting || cq->status == reqTransfer) {
				if (time(0) - cq->start_time > MAX_TIMEOUT)
					cq->status = reqError;

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
	for (int i = 0; i < max_dns_inflight; i++)
		pthread_join (dns_workers[i], NULL);
}

void db_reconnect(MYSQL ** c) {
	if (*c)
		mysql_close(*c);

	char *server = getenv("MYSQL_HOST");
	char *user = getenv("MYSQL_USER");
	char *password = getenv("MYSQL_PASS");
	char *database = getenv("MYSQL_DB");
	
	*c = mysql_init(NULL);
	// Eanble auto-reconnect, as some versions do not enable it by default
	my_bool reconnect = 1;
	mysql_options(*c, MYSQL_OPT_RECONNECT, &reconnect);
	if (!mysql_real_connect(*c, server, user, password, database, 0, NULL, 0)) {
		fprintf(stderr, "User %s Pass %s Host %s Database %s\n", user, password, server, database);
		exit(1);
	}
}

void mysql_initialize() {
	/* Connect to database */
	printf("Connecting to mysqldb...\n");
	db_reconnect(&mysql_conn_select);
	db_reconnect(&mysql_conn_update);
	db_reconnect(&mysql_conn_update2);
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
	char * newbuffer = (char*)malloc(*size+32);
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

// Wait for DNS requests, process them one at a time and store the IP back
void * dns_dispatcher(void * args) {
	int offset = (int)args;

	while (!adder_finish) {
		struct connection_query * cquery = NULL;
		for (int i = offset; i < MAX_OUTSTANDING_QUERIES; i += max_dns_inflight) {
			if (connection_table[i].status == reqDnsQuery) {
				if (time(0) - connection_table[i].start_time > MAX_TIMEOUT)
					connection_table[i].status = reqError;
			}

			if (connection_table[i].status == reqDnsQuery) {
				cquery = &connection_table[i];
				cquery->status = reqDnsQuerying;
				break;
			}
		}

		if (cquery) {
			struct addrinfo *result;
			std::string tosolve = gethostname(cquery->url);
			if (getaddrinfo(tosolve.c_str(), NULL, NULL, &result) == 0) {
				unsigned long ip = ntohl(((struct sockaddr_in*)result->ai_addr)->sin_addr.s_addr);
				cquery->ip = ip;
				cquery->start_time = time(0);
				std::cerr << "Resolved " << tosolve << " to " << ip << std::endl;
				cquery->status = reqConnecting;
			}
		}
		else
			sleep(1);
	}
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
			if (cquery->status == reqComplete || cquery->status == reqCompleteError) {
				// Analyze the response, maybe we find some redirects
				dechunk_http(cquery->inbuffer,&cquery->received);
				std::string newloc = parse_response((char*)cquery->inbuffer, cquery->received, cquery->url);
				
				if (newloc != "" && cquery->redirs < MAX_REDIRS) {
					// Reuse the same entry for the request
					cquery->retries = 0;
					cquery->redirs++;
					cquery->tosend_offset = 0;
					cquery->received = 0;
					cquery->inbuffer = (char*)realloc(cquery->inbuffer,READBUF_CHUNK+32);
					memset(cquery->inbuffer,0,READBUF_CHUNK+32);
					close(cquery->socket);
					cquery->socket = socket(AF_INET, SOCK_STREAM, 0);
					cquery->start_time = time(0);
					setNonblocking(cquery->socket);

					// FIXME: Check for HTTPS and submit this element to secondchance queue
					free(cquery->outbuffer);
					std::string path = getpathname(newloc);
					std::string host = gethostname(newloc);

					cquery->outbuffer = strdup(generateHTTPQuery(host, path).c_str());
					cquery->tosend_max = strlen(cquery->outbuffer);
					cquery->url = newloc;
					std::cout << "Query " << cquery->url << std::endl;

					cquery->status = (cquery->url.substr(0,7) == "http://") ? reqDnsQuery : reqDnsQuery; // FIXME
				}
				else{ 
					if (bannercrawl) {
						char tempb[cquery->received*2+2];
						mysql_real_escape_string(mysql_conn_update, tempb, cquery->inbuffer, cquery->received);
						sprintf(sql_query, "UPDATE services SET `head`='%s',`status`=`status`&(~1) WHERE `ip`=%d AND `port`=%d\n",
							tempb, cquery->ip, cquery->port);
					}else{
						int eflag = cquery->status == reqCompleteError ? 0x80 : 0;
						char tempb [cquery->received*2+2];
						char tempb_[cquery->received*2+2];
						int len1, len2; char *p1, *p2;
						separate_body(cquery->inbuffer, cquery->received, &p1, &p2, &len1, &len2);

						mysql_real_escape_string(mysql_conn_update, tempb,  p1, len1);
						mysql_real_escape_string(mysql_conn_update, tempb_, p2, len2);

						std::string hostname = mysql_real_escape_std_string(mysql_conn_update,cquery->vhost);
						std::string url      = mysql_real_escape_std_string(mysql_conn_update,cquery->url);

						sprintf(sql_query, "UPDATE virtualhosts SET `index`='%s',`head`='%s',`url`='%s',`status`=(`status`&(~1))|%d WHERE `ip`=%d AND `host`=\"%s\";", tempb_, tempb, url.c_str(), eflag, cquery->ip, hostname.c_str());
					}
					num_processed++;
					if (mysql_query(mysql_conn_update,sql_query)) {
						if (mysql_errno(mysql_conn_update) == CR_SERVER_GONE_ERROR) {
							printf("Reconnect due to connection drop\n");
							db_reconnect(&mysql_conn_update);
						}
					}

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
}


// CURL worker thread
struct http_query {
	char * buffer;
	int received;
};
static size_t curl_fwrite(void *buffer, size_t size, size_t nmemb, void *stream);

void * curl_dispatcher(void * args) {
	int num_thread = (int)args;
	while (!adder_finish) {
		// Look for successful or failed transactions
		int found = 0;
		for (int i = num_thread; i < MAX_OUTSTANDING_QUERIES; i += curl_workers) {
			struct connection_query * cquery = &connection_table[i];
			
			if (cquery->status == reqCurl) {
				cquery->status == reqCurlTransfer;

				struct http_query hq;
				memset(&hq,0,sizeof(hq));
				hq.buffer = (char*)malloc(32);

				CURL * curl = curl_easy_init();
				curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0);
				curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0);
				curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1);
				curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_fwrite);
				curl_easy_setopt(curl, CURLOPT_WRITEDATA, &hq);
				curl_easy_setopt(curl, CURLOPT_URL, cquery->url.c_str());

				std::cerr << "cURL Fetch " << cquery->url << std::endl;
				CURLcode res = curl_easy_perform(curl);
				
				// Move data
				cquery->inbuffer = hq.buffer;
				cquery->received = hq.received;

				if(CURLE_OK == res)
					cquery->status = reqComplete;
				else
					cquery->status = reqError;

				curl_easy_cleanup(curl);
			}
		}
	}
	
	std::cerr << "Closing cURL thread" << std::endl;
}


static size_t curl_fwrite(void *buffer, size_t size, size_t nmemb, void *stream) {
	struct http_query * q = (struct http_query *)stream;

	q->buffer = (char*)realloc(q->buffer, size*nmemb + q->received + 32);
	char *bb = q->buffer;

	memcpy(&bb[q->received], buffer, size*nmemb);
	q->received += size*nmemb;

	//if (q->received > MAX_BUFFER_SIZE)
	//	return -1;  // Ops!

	return size*nmemb;
}



