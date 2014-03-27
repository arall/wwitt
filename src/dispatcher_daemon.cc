
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <curl/curl.h>
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
// UUID\n
// URL\n
// Req size in ASCII form\n
// Request
// Responses are in the form:
// UUID\n
// response size (head+body) in ASCII form \n
// head+body (bunch of bytes)

#define BUFSIZE (1024*1024)   // maximum size of any datagram(16 bits the size of identifier)
#define TRUE 1
#define FALSE 0
#define default_low  1
#define default_high 1024

#define READBUF_CHUNK 4096

#define MAX_TIMEOUT     10   // In seconds
#define MAX_DNS_TIMEOUT 10   // In seconds
#define MAX_RETRIES     10
#define MAX_REDIRS      10

#define MAX_OUTSTANDING_QUERIES (1024*64)

int max_inflight = 100;
int dns_workers = 20;
int curl_workers = 20;


std::string output_stream;

// Each worker (DNS or HTTP) is responsible for killing jobs, this is, it is the only writer (to avoid mutexes)
enum RequestsStatus {
	reqEmpty,
	reqDnsQuery, reqDnsQuerying, 
	reqConnecting, reqTransfer, 
	reqCurl, reqCurlTransfer,
	reqComplete, reqCompleteError, reqError
};

struct connection_query {
	unsigned long long uuid;
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
	c->uuid = 0;
}

#define CONNECT_ERR(ret) (ret < 0 && errno != EINPROGRESS && errno != EALREADY && errno != EISCONN)
#define CONNECT_OK(ret) (ret >= 0 || (ret < 0 && (errno == EISCONN || errno == EALREADY)))
#define IOTRY_AGAIN(ret) (ret < 0 && (errno == EAGAIN || errno == EWOULDBLOCK))

void * dns_dispatcher(void * args);
void * curl_dispatcher(void * args);
void dechunk_http(char * buffer, int * size);
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

std::string long_to_str (long long a) {
	char temp[256];
	sprintf(temp,"%lld",a);
	return std::string(temp);
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

bool next_query(connection_query * cq, std::string & rbuf) {
	size_t ptrs[3];
	for (int i = 0; i < sizeof(ptrs)/sizeof(ptrs[0]); i++) {
		ptrs[i] = rbuf.find("\n", i > 0 ? ptrs[i-1]+1 : 0);
		if (ptrs[i] == std::string::npos)
			return false;
	}
	cq->uuid = atol(rbuf.substr(0,ptrs[0]).c_str());
	cq->url  = rbuf.substr(ptrs[0]+1,ptrs[1]-2);
	long long rsize = atol(rbuf.substr(ptrs[1]+1,ptrs[2]-ptrs[1]-1).c_str());
	if (rbuf.size() - ptrs[2] - 1 < rsize)
		return false;

	std::string req = rbuf.substr(ptrs[2]+1, rsize);
	cq->outbuffer = (char*)malloc(req.size());
	memcpy(cq->outbuffer, req.c_str(), req.size());
	cq->tosend_max = req.size();

	rbuf = rbuf.substr(ptrs[2] + 1 + rsize);
	return true;
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
"           HTTP/HTTPS dispatcher daemon                  \n"  );

	
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
	if (inpipefd < 0) { perror("Could not open IN pipe: "); exit(1); }
	int outpipefd = open(argv[2], O_NONBLOCK | O_WRONLY);
	
	signal(SIGPIPE, SIG_IGN);
	
	int bannercrawl = (strcmp("banner",argv[1]) == 0);
	
	// Initialize structures
	for (int i = 0; i < sizeof(connection_table)/sizeof(connection_table[0]); i++)
		clear_query(&connection_table[i]);
	
	// Start!
	pthread_t dns_worker_thread[dns_workers];
	for (int i = 0; i < dns_workers; i++)
		pthread_create (&dns_worker_thread[i], NULL, &dns_dispatcher, (void*)(uintptr_t)i);
	pthread_t curl_worker_thread[curl_workers];
	for (int i = 0; i < curl_workers; i++)
		pthread_create (&curl_worker_thread[i], NULL, &curl_dispatcher, (void*)(uintptr_t)i);

	std::string request_buffer;

	int num_active = 0;	
	// Infinite loop: query IP/Domain blocks
	while (1) {
	
		// Look for more requests coming from in-pipe (only if we have room enough)
		if (num_active < max_inflight) {
			if (request_buffer.size() < 64*1024) { // Do not query more stuff if we have some already
				char tempb[16*1024];
				int r = read(inpipefd, tempb, sizeof(tempb)-1);
				if (r >= 0)
					request_buffer += std::string(tempb, r);
				else if (!IOTRY_AGAIN(r)) {
					fprintf(stderr, "Could not read from request pipe, shutting down\n");
					perror("Error:");
					exit(1);
				}
			}
			
			connection_query reqq;
			size_t p = request_buffer.find("\n");
			while (next_query(&reqq, request_buffer)) {
				std::string url = reqq.url;
				std::cerr << "Scheduling " << url << std::endl;
				
				for (int i = 0; i < MAX_OUTSTANDING_QUERIES; i++) {
					struct connection_query * cquery = &connection_table[i];
					if (cquery->status == reqEmpty) {
						*cquery = reqq;
						cquery->url = url;
						
						// Select between HTTP/HTTPS
						if (url.substr(0,7) == "http://")
							cquery->status = reqDnsQuery;
						else
							cquery->status = reqCurl;
						break;
					}
				}
			}
		}
		
		// Try to output some data to the output pipe
		if (outpipefd < 0) {
			// Try to open it...
			outpipefd = open(argv[2], O_NONBLOCK | O_WRONLY);
			if (outpipefd < 0)
				output_stream = ""; // Discard data until we have the descriptoo wide open
		}
		
		if (output_stream.size() > 0) {
			int w = write(outpipefd, output_stream.c_str(), output_stream.size());
			if (w >= 0)
				output_stream = output_stream.substr(w);
			else if (!IOTRY_AGAIN(w)) {
				fprintf(stderr, "Could not write to output request pipe, this will discard data so on\n");
				outpipefd = -1;
			}
		}
		
		// Connection loop
		num_active = 0;
		struct connection_query * cq;
		for (cq = &connection_table[0]; cq != &connection_table[MAX_OUTSTANDING_QUERIES]; cq++) {
			if (cq->status == reqConnecting) {
				if (!cq->socket) {
					cq->socket = socket(AF_INET, SOCK_STREAM, 0);
					setNonblocking(cq->socket);
					cq->inbuffer = (char*)malloc(READBUF_CHUNK+32);
					std::string path = getpathname(cq->url);
					std::string host = gethostname(cq->url);

					//cq->outbuffer = strdup(generateHTTPQuery(host, path).c_str());
					//cq->tosend_max = strlen(cq->outbuffer);
					std::cerr << "Connecting " << cq->ip << std::endl;
				}

				struct sockaddr_in sin;
				sin.sin_port = htons(cq->port);
				sin.sin_addr.s_addr = htonl(cq->ip);
				sin.sin_family = AF_INET;
			
				int res = connect(cq->socket,(struct sockaddr *)&sin,sizeof(sin));
				if (CONNECT_ERR(res)) {
					// Drop connection!
					std::cerr << "Connection dropped " << cq->ip << " error " << errno << std::endl;
					cq->status = reqError;
				}
				else if (CONNECT_OK(res)) {
					std::cerr << "Connected " << cq->ip << std::endl;
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
					if (cq->tosend_offset == cq->tosend_max)
						std::cerr << "Request send " << cq->ip << std::endl;
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
					else {
						cq->status = reqComplete; // Disconnect, OK!
						std::cerr << "Complete! " << cq->ip << std::endl;
					}
				}
				else if (!IOTRY_AGAIN(rec)) {
					// Connection error!
					cq->status = reqError;
				}
			}
			
			if (cq->status == reqError) {
				if (cq->retries++ < MAX_RETRIES) {
					cq->tosend_offset = 0;
					cq->received = 0;
					cq->start_time = time(0);
					
					cq->status = (cq->url.substr(0,7) == "http://") ? reqDnsQuery : reqCurl;
				}
				else {
					// We are done with this guy, mark is as false ready
					cq->status = reqCompleteError;
				}
			}
			if (cq->status == reqComplete) {
				// Look for redirects :)
				dechunk_http ((char*)cq->inbuffer, &cq->received);
				std::string newurl = parse_response((char*)cq->inbuffer, cq->received, cq->url);
				
				if (newurl == "") {
					output_stream += long_to_str(cq->uuid) + "\n";
					output_stream += long_to_str(cq->received) + "\n";
					output_stream += std::string (cq->inbuffer, cq->received);
					clear_query(cq);
				}else{
					std::cerr << "Redirect to " << newurl << std::endl;
					clear_query(cq);
					cq->url = newurl;
					
					cq->status = (cq->url.substr(0,7) == "http://") ? reqDnsQuery : reqCurl;
				}
				
			}
			if (cq->status == reqCompleteError) {
				clear_query(cq);
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
	for (int i = 0; i < curl_workers; i++)
		pthread_join (curl_worker_thread[i], NULL);
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


// DNS worker thread
void * dns_dispatcher(void * args) {
	uintptr_t num_thread = (uintptr_t)args;
	while (!adder_finish) {
		// Look for successful or failed transactions
		int found = 0;
		for (int i = num_thread; i < MAX_OUTSTANDING_QUERIES; i += dns_workers) {
			struct connection_query * cquery = &connection_table[i];
			
			if (cquery->status == reqDnsQuery) {
			
				if (time(0) - cquery->start_time > MAX_DNS_TIMEOUT) {
					cquery->status = reqError;
				}
				else {
					cquery->status = reqDnsQuerying;
				
					std::string host = gethostname(cquery->url);
				
					// DNS solve ...
					struct addrinfo *result;
					if (getaddrinfo(host.c_str(), NULL, NULL, &result) == 0) {
						unsigned long ip = ntohl(((struct sockaddr_in*)result->ai_addr)->sin_addr.s_addr);
						cquery->ip = ip;
						cquery->port = 80;
						cquery->start_time = time(0);
						std::cerr << "Resolved " << host << " to " << ip << std::endl;
					}

					cquery->status = reqConnecting;
				
					found = 1;
				}
			}
		}
		if (!found)
			sleep(1);
	}
	
	printf("DNS daemon quitting!\n");
}

// CURL worker thread
struct http_query {
	char * buffer;
	int received;
};
static size_t curl_fwrite(void *buffer, size_t size, size_t nmemb, void *stream);

void * curl_dispatcher(void * args) {
	uintptr_t num_thread = (uintptr_t)args;
	while (!adder_finish) {
		// Look for successful or failed transactions
		int found = 0;
		for (int i = num_thread; i < MAX_OUTSTANDING_QUERIES; i += curl_workers) {
			struct connection_query * cquery = &connection_table[i];
			
			if (cquery->status == reqCurl) {
				cquery->status == reqCurlTransfer;
				found = 1;

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
		if (!found)
			sleep(1);
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

