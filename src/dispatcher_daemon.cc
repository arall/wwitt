
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <string>
#include <iostream>
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
#include <sys/resource.h> 

// Requests have the format:
// URL\n
// Responses are in the form:
// URL\n
// response size (head+body) in ASCII form \n
// head+body (bunch of bytes)

#define BUFSIZE (1024*1024)   // maximum size of any datagram(16 bits the size of identifier)
#define TRUE 1
#define FALSE 0
#define default_low  1
#define default_high 1024

#define READBUF_CHUNK 4096

#define MAX_TIMEOUT 10   // In seconds
#define MAX_RETRIES 10
#define MAX_REDIRS  10

#define MAX_OUTSTANDING_QUERIES (1024*64)

// Each worker (DNS or HTTP) is responsible for killing jobs, this is, it is the only writer (to avoid mutexes)
enum RequestsStatus { reqEmpty, reqDnsQuery, reqDnsQuerying, reqConnecting, reqTransfer, reqComplete, reqCompleteError, reqError };

struct connection_query {
	RequestsStatus status;

	int socket;
	unsigned int   ip;
	unsigned short port;
	unsigned char  retries;
	unsigned char  redirs;
	
	std::string url;
	
	int tosend_max, tosend_offset;
	int received;
	char *inbuffer, *outbuffer;
	unsigned long start_time;
} connection_table[MAX_OUTSTANDING_QUERIES];
struct pollfd poll_desc[MAX_OUTSTANDING_QUERIES];

void clear_query(connection_query * c) {
	c->status = reqEmpty;
	c->url = "";
	if (c->inbuffer) free(c->inbuffer);
	if (c->outbuffer) free(c->outbuffer);
	c->inbuffer = c->outbuffer = 0;
	c->tosend_max = c->tosend_offset = c->received = 0;
	c->start_time = time(0);
	c->socket = 0;
	c->ip = 0; c->port = 0;
	c->redirs = c->retries = 0;
}

#define CONNECT_ERR(ret) (ret < 0 && errno != EINPROGRESS && errno != EALREADY && errno != EISCONN)
#define CONNECT_OK(ret) (ret >= 0 || (ret < 0 && errno == EISCONN))
#define IOTRY_AGAIN(ret) (ret < 0 && (errno == EAGAIN || errno == EWOULDBLOCK))

void * dns_dispatcher(void * args);
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

std::string generateHTTPQuery(const std::string & vhost, const std::string & path) {
	std::string p = (path == "") ? "/" : path;
	
	return "GET " + p + " HTTP/1.1\r\nHost: " + vhost + "\r\nUser-Agent: Mozilla/5.0 (Windows; U; Windows NT 5.1; rv:22.0) Gecko/20100101 Firefox/22.0\r\nConnection: close\r\n\r\n";
}

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
	char * ret = (char*)calloc(4096,1);
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

int main(int argc, char **argv) {
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
"              HTTP dispatcher daemon                  \n"  );

	
	if ( argc < 3 ) {
		fprintf(stderr, "Usage: %s request_pipe response_pipe\n", argv[0]);
		exit(1);
	}
	
	struct rlimit limit; limit.rlim_cur = MAX_OUTSTANDING_QUERIES*1.1f; limit.rlim_max = MAX_OUTSTANDING_QUERIES*1.6f;
	if (setrlimit(RLIMIT_NOFILE, &limit)<0) {
		fprintf(stderr, "setrlimit failed!\n");
		exit(1);
	}
	
	// Open server PIPE
	int inpipefd  = open(argv[1], O_NONBLOCK | O_RDONLY);
	int outpipefd = open(argv[2], O_NONBLOCK | O_WRONLY);
	
	signal(SIGPIPE, SIG_IGN);
	
	int max_inflight = 100;
	int dns_workers = 20;
	int bannercrawl = (strcmp("banner",argv[1]) == 0);
	
	// Initialize structures
	for (int i = 0; i < sizeof(connection_table)/sizeof(connection_table[0]); i++)
		clear_query(&connection_table[i]);
	
	// Start!
	pthread_t dns_worker_thread[dns_workers];
	for (int i = 0; i < dns_workers; i++)
		pthread_create (&dns_worker_thread[i], NULL, &dns_dispatcher, NULL);

	std::string request_buffer;

	int num_active = 0;	
	// Infinite loop: query IP/Domain blocks
	while (1) {
	
		// Look for more requests coming from in-pipe (only if we have room enough)
		if (num_active < max_inflight) {
			if (request_buffer.size() < 64*1024) { // Do not query more stuff if we have some already
				char tempb[16*1024];
				int r = read(inpipefd, tempb, sizeof(tempb)-1);
				if (r > 0)
					request_buffer += std::string(tempb, r);
				else if (!IOTRY_AGAIN(r)) {
					fprintf(stderr, "Could not read from request pipe, shutting down\n");
					exit(1);
				}
			}
			
			size_t p = request_buffer.find("\n");
			while (p != std::string::npos) {
				std::string url = request_buffer.substr(0,p);
				
				for (int i = 0; i < MAX_OUTSTANDING_QUERIES; i++) {
					struct connection_query * cquery = &connection_table[i];
					if (cquery->status == reqEmpty) {
						cquery->url = url;
						cquery->status = reqDnsQuery;
						break;
					}
				}
				
				request_buffer = request_buffer.substr(p+1);
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
					clear_query(cq);
					cq->received = 0;
					cq->status = reqCompleteError;
				}
			}
			if (cq->status == reqConnecting || cq->status == reqTransfer) {
				if (time(0) - cq->start_time > MAX_TIMEOUT)
					cq->status = reqError;

				int mask = POLLERR;
				// Needs to output request if we are connecting or connected but not finished sending
				mask |= (cq->tosend_offset < cq->tosend_max) ? POLLOUT : 0;
				// Needs to input if we are connecting or we finished writing
				mask |= (cq->tosend_offset == cq->tosend_max || cq->status == reqConnecting) ? POLLIN : 0;

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
	
	for (int i = 0; i < dns_workers; i++)
		pthread_join (dns_worker_thread[i], NULL);
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

void * dns_dispatcher(void * args) {
	while (!adder_finish) {
		// Look for successful or failed transactions
		int found = 0;
		for (int i = 0; i < MAX_OUTSTANDING_QUERIES; i++) {
			struct connection_query * cquery = &connection_table[i];
			if (time(0) - cquery->start_time > MAX_TIMEOUT) {
					cquery->status = reqError;
			}
			
			if (cquery->status == reqDnsQuery) {
				
				cquery->status = reqDnsQuerying;
				
				std::string host = gethostname(cquery->url);
				
				// DNS solve ...
				struct addrinfo *result;
				if (getaddrinfo(host.c_str(), NULL, NULL, &result) == 0) {
					char hostname[2048];
					if (getnameinfo(result->ai_addr, result->ai_addrlen, hostname, 2047, NULL, 0, 0) == 0) {
						struct in_addr ipa;
						inet_aton(hostname, &ipa);
						cquery->ip = ntohl(ipa.s_addr);
						cquery->start_time = time(0);
						std::cerr << "Resolved " << host << " to " << hostname << std::endl;
					}
				}

				// cquery->ip = ...
				
				found = 1;
			}
		}
		if (!found)
			sleep(1);
	}
	
	printf("DNS daemon quitting!\n");
}


