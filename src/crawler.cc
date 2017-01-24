
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <assert.h>
#include <curl/curl.h>
#include <iostream>
#include <vector>
#include <sys/epoll.h>
#include <sys/poll.h>
#include <fcntl.h>
#include <atomic>
#include <mutex>
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
#include <sys/resource.h>
#include <ares.h>
#include "pqueue.h"
#include "crawler.h"
//#include "crawler_mysql.h"
#include "crawler_kc.h"

/*
 3 paths: 
     banner (new->connect->complete)
     http   (new->dns->connect->complete)
     https  (new->curl->complete)

  backpath: from any state to new_queries when error is found
 
 new_queries  ---> dns_queries  ---> connection_query  ---> completed_queries
               |                 ^                      ^
               ------------------!                      |
               |                                        |
               ----------> curl_queries  ---------------!
*/

#define TRUE  1
#define FALSE 0
#define default_low  1
#define default_high 1024

#define READBUF_CHUNK (16*1024)

#define MAX_DNSTIMEOUT   5
#define MAX_TIMEOUT     10   // In seconds
#define MAX_RETRIES      5   // More than enough
#define MAX_REDIRS      10

#define MAX_EPOLL_RET           (4*1024)
#define MAX_OUTSTANDING_QUERIES (1024*256)

#define CONNECT_ERR(ret) (ret < 0 && errno != EINPROGRESS && errno != EALREADY && errno != EISCONN)
#define CONNECT_OK(ret) (ret >= 0 || (ret < 0 && (errno == EISCONN || errno == EALREADY)))
#define IOTRY_AGAIN(ret) (ret < 0 && (errno == EAGAIN || errno == EWOULDBLOCK))

enum RequestsStatus {
	reqPending,
	reqDnsQuery, reqDnsQuerying,   // DNS
	reqConnecting, reqTransfer,    // Epoll work
	reqCurl, reqCurlTransfer,      // Curl
	reqComplete, reqCompleteError, // DB work
	reqError
};

volatile int verbose = 0;
int epollfd;

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

std::atomic<unsigned> dns_inflight_(0);
std::atomic<unsigned> curl_inflight_(0);
std::atomic<unsigned> num_completed(0);
std::atomic<unsigned> num_queued(0);

pqueue new_queries;
pqueue dns_queries;
pqueue curl_queries;
pqueue connection_queries;
pqueue completed_queries;

// Returns http[s]://domain.com/   given any URL under that website (base URL as its name indicates)
std::string base_url (const std::string & burl) {
	unsigned int s = 0;
	if (burl.substr(0,7) == "http://") s = 7;
	if (burl.substr(0,8) == "https://") s = 8;
	while (s < burl.size() && burl[s] != '/')
		s++;
	
	if (s == burl.size()) return burl + "/";
	return burl.substr(0,s+1);
}
std::string gethostname(std::string url) {
	if (url.substr(0,7) == "http://")  url = url.substr(7);
	if (url.substr(0,8) == "https://") url = url.substr(8);
	
	size_t p = url.find("/");
	if (p == std::string::npos)
		return url;
	else
		return url.substr(0, p);
}
std::string getpathname(std::string url) {
	if (url.substr(0,7) == "http://")  url = url.substr(7);
	if (url.substr(0,8) == "https://") url = url.substr(8);
	
	size_t p = url.find("/");
	if (p == std::string::npos)
		return "/";
	else
		return url.substr(p);
}
std::string generateHTTPQuery(const std::string vhost, const std::string path) {
	std::string p = (path == "") ? "/" : path;

	return "GET " + p + " HTTP/1.1\r\nHost: " + vhost +  \
		"\r\nUser-Agent: Mozilla/5.0 (Windows; U; Windows NT 5.1; rv:22.0) Gecko/20100101 Firefox/22.0\r\nConnection: close\r\n\r\n";
}

class Connection {
public:
	static uint64_t gennumber;
	static std::map<uint64_t, Connection*> conns;
	static std::mutex conns_mutex;

	Connection() :
	   status(reqPending), retries(0),
	   redirs(0), dechunked(false)
	{
		sock = socket(AF_INET, SOCK_STREAM, 0);
		setNonblocking(sock);
		time(&start_time);
		gid = gennumber++;
	}

	Connection(std::string virthost, std::string url) : Connection() {
		std::string path = getpathname(url);
		std::string host = gethostname(url);
		outbuffer = generateHTTPQuery(host, path);

		port = 80;
		this->vhost = virthost;
		this->url = url;
		if (verbose)
			std::cout << "Query " << url << " (" << host << "," << path << ")" << std::endl;
	}

	Connection(uint32_t ip, unsigned port) : Connection() {
		this->ip = ip;
		this->port = port;
	}

	static Connection* redirConnection(Connection *oc, std::string newloc) {
		// Use the same old vhost, just give it the new url
		Connection *c = new Connection(oc->vhost, newloc);
		c->redirs = oc->redirs + 1;
		c->retries = oc->retries;
		delete oc;
		return c;
	}

	static Connection* retryConnection(Connection *oc, bool bannercrawl) {
		// Just copy the initial values and increment retries
		Connection *c = bannercrawl ? new Connection(oc->ip, oc->port) :
		                              new Connection(oc->vhost, oc->url);
		c->retries = oc->retries + 1;
		c->redirs = oc->redirs;
		delete oc;
		return c;
	}

	~Connection() {
		if (sock >= 0)
			close(sock);
		epoll_ctl(epollfd, EPOLL_CTL_DEL, sock, NULL);
		{
			std::lock_guard<std::mutex> lock(conns_mutex);
			conns.erase(gid);
		}
	}

	bool hasTimedOut() const {
		return (time(0) - start_time > MAX_TIMEOUT);
	}

	void doConnect() {
		status = reqConnecting;

		struct sockaddr_in sin;
		sin.sin_port = htons(port);
		sin.sin_addr.s_addr = htonl(ip);
		sin.sin_family = AF_INET;
	
		int res = connect(sock, (struct sockaddr *)&sin,sizeof(sin));
		if (CONNECT_ERR(res))
			status = reqError; // Drop connection!
		else if (CONNECT_OK(res))
			status = reqTransfer;
	}

	// Returns whether process has finished
	bool process(bool bannercrawl) {
		if (status == reqConnecting)
			// Keep retrying connect till we succeed
			doConnect();
		if (status == reqTransfer) {
			// Try to send data (if any)
			if (outbuffer.size()) {
				int sent = write(sock, outbuffer.c_str(), outbuffer.size());
				if (sent > 0)
					outbuffer = outbuffer.substr(sent);
				else if (!IOTRY_AGAIN(sent))
					status = reqError;
			}
		
			// Try to receive data
			char temp[READBUF_CHUNK];
			int rec = read(sock, temp, READBUF_CHUNK);
			if (rec > 0) {
				if (inbuffer.size() < BUFSIZE)
					inbuffer.append(temp, rec);
			}
			else if (rec == 0) {
				// End of data. OK if we sent all the data only
				if (outbuffer.size()) 
					status = reqError; // Disconnect, but ERR!
				else
					status = reqComplete; // Disconnect, OK!
			}
			else if (!IOTRY_AGAIN(rec))
				status = reqError;  // Connection error!
		}
		
		// Check timeout, is OK for banner, not for HTTP
		if (hasTimedOut()) {
			if (bannercrawl && inbuffer.size() > 0)
				status = reqComplete;
			else
				status = reqError;
		}

		updateEpoll();

		return (status == reqComplete || status == reqError);
	}

	void updateEpoll() {
		if (status == reqConnecting || status == reqTransfer) {
			struct epoll_event ev;
			ev.data.ptr = this;
			ev.events = (outbuffer.size()) ? (POLLOUT|POLLIN) : POLLIN;
			if (epoll_ctl(epollfd, EPOLL_CTL_ADD, sock, &ev) < 0)
				epoll_ctl(epollfd, EPOLL_CTL_MOD, sock, &ev);
		} else {
			epoll_ctl(epollfd, EPOLL_CTL_DEL, sock, NULL);
		}
	}

	uint64_t gid;
	int sock;
	uint32_t ip;
	unsigned short port;
	RequestsStatus status;
	unsigned char retries;
	unsigned char redirs;
	bool dechunked;
	
	std::string vhost;
	std::string url;
	std::string inbuffer, outbuffer;
	time_t start_time;
};

uint64_t Connection::gennumber = 0;
std::map<uint64_t, Connection*> Connection::conns;
std::mutex Connection::conns_mutex;

void * database_dispatcher(void * args);
void * dns_dispatcher(void * args);
void * curl_dispatcher(void * args);

int max_inflight = 100;
int max_curl_inflight = 50;

std::atomic<bool> db_global_end(false);
void sigterm(int s) {
	std::cerr << "Forcing end ..." << std::endl;
	db_global_end = true;
}

DBIface *db = NULL;

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
"                Web/Banner crawler                  \n"  );


	fprintf(stderr, "Using %u DNS servers\n", sizeof(dns_servers)/sizeof(dns_servers[0]));
	
	if ( argc < 3 ) {
		fprintf(stderr, "Usage: %s http   db_args [max-requests]\n", argv[0]);
		fprintf(stderr, "Usage: %s banner db_args [max-requests]\n", argv[0]);
		exit(1);
	}
	if (argc == 4)
		max_inflight = atoi(argv[3]);
	
	struct rlimit limit; limit.rlim_cur = MAX_OUTSTANDING_QUERIES*1.1f; limit.rlim_max = MAX_OUTSTANDING_QUERIES*1.6f;
	if (setrlimit(RLIMIT_NOFILE, &limit)<0) {
		fprintf(stderr, "setrlimit failed!\n");
		exit(1);
	}
	
	bool bannercrawl = (strcmp("banner",argv[1]) == 0);
	
	signal(SIGPIPE, SIG_IGN);
	signal(SIGTERM, &sigterm);
	signal(SIGINT,  &sigterm);

	epollfd = epoll_create(1);

	std::cout << "Loading DB ... " << std::endl;
	db = new DBKC(bannercrawl, argv[2]);
	std::cout << "DB loaded!" << std::endl;
	
	// Init queues
	pqueue_init(&new_queries);
	pqueue_init(&dns_queries);
	pqueue_init(&curl_queries);
	pqueue_init(&connection_queries);
	pqueue_init(&completed_queries);
	
	// Start!
	pthread_t dbt;
	pthread_create (&dbt, NULL, &database_dispatcher, &bannercrawl);

	pthread_t dns_worker;
	pthread_t curl_workers[max_curl_inflight];
	if (!bannercrawl) {
		for (int i = 0; i < max_curl_inflight; i++)
			pthread_create (&curl_workers[i], NULL, &curl_dispatcher, 0);
		
		pthread_create (&dns_worker, NULL, &dns_dispatcher, 0);
	}
	
	int db_end = 0;
	// Infinite loop: query IP/Domain blocks
	
	// Do this while DB has data and we have inflight requests
	while ((num_queued != num_completed || (!db_end && !db_global_end))) {
	
		unsigned numconns;
		{
			std::lock_guard<std::mutex> lock(Connection::conns_mutex);
			numconns = Connection::conns.size();
		}
		printf("\rQueues> New: %d, Completed: %d, DNS: %d, Curl: %d, Connection: %d // INFLIGHT %u, DONE %u, DB: %u        ", 
			pqueue_size(&new_queries), pqueue_size(&completed_queries),
			pqueue_size(&dns_queries) + dns_inflight_,
			pqueue_size(&curl_queries) + curl_inflight_,
			pqueue_size(&connection_queries) + numconns,
			num_queued - num_completed, unsigned(num_completed), (unsigned)db_end);
		fflush(stdout);

		if (num_completed > 50000000)
			db_global_end = 1;
		
		// Generate queries and generate new connections
		if (num_queued - num_completed < max_inflight && !db_end && !db_global_end) {
			db_end = 1;
			std::vector <std::string> row;
			while (db->next(row)) {
				db_end = 0;
				Connection *t = bannercrawl ? new Connection(atoi(row[0].c_str()), atoi(row[1].c_str())) :
				                              new Connection(row[0], "http://" + row[0] + "/");
			
				// Now queue it as a new request
				pqueue_push(&new_queries, t);
				num_queued++;

				// Do not full queues!
				if (num_queued - num_completed >= max_inflight || db_global_end)
					break;
			}
		}
		
		// Read requests from the new queue and cathegorize them
		Connection *icq = (Connection*)pqueue_pop_nonb(&new_queries);
		while (icq) {
			// It may be new or a respin, check retries and redirs
			if (icq->retries++ >= MAX_RETRIES) {
				// We are done with this guy, mark is as false ready
				icq->inbuffer.clear();
				icq->status = reqCompleteError;
				pqueue_push(&completed_queries, icq);
			}
			else {
				// Good query, send it to the best suited queue
				if (bannercrawl)
					icq->status = reqConnecting;
				else {
					icq->status = (icq->url.substr(0,7) == "http://") ? reqDnsQuery : reqCurl;
					if (verbose)
						std::cout << "Query " << icq->url << std::endl;
				}
				if (icq->status == reqDnsQuery)   pqueue_push(&dns_queries, icq);
				if (icq->status == reqCurl)       pqueue_push(&curl_queries, icq);
				//if (icq->status == reqConnecting) active_connections.push_back(icq);
			}
						
			icq = (Connection*)pqueue_pop_nonb(&new_queries);
		}

		// Read replies from DNS
		icq = (Connection*)pqueue_pop_nonb(&connection_queries);
		while (icq) {
			icq->doConnect();
			icq->updateEpoll();
			{
				std::lock_guard<std::mutex> lock(Connection::conns_mutex);
				Connection::conns[icq->gid] = icq;
			}
			icq = (Connection*)pqueue_pop_nonb(&connection_queries);
		}
		
		// Waiting. Poll for sockets
		struct epoll_event events[MAX_EPOLL_RET];
		int ne = epoll_wait(epollfd, events, MAX_EPOLL_RET, 1000);

		std::set<Connection*> remove_set;

		// Clear timeout stuff, things are sorted by start time
		{
			std::lock_guard<std::mutex> lock(Connection::conns_mutex);
			for (auto elem: Connection::conns)
				if (elem.second->hasTimedOut()) {
					if (elem.second->process(bannercrawl))
						remove_set.insert(elem.second);
				}
				else
					break;
		}

		// Process events in the waitqueue
		for (int i = 0; i < ne; i++) {
			Connection * c = (Connection*)events[i].data.ptr;
			if (c->process(bannercrawl))
				remove_set.insert(c);
		}

		for (auto it: remove_set) {
			{
				std::lock_guard<std::mutex> lock(Connection::conns_mutex);
				Connection::conns.erase(it->gid);
			}

			// Handle error (respin) and complete (send to complete)
			if (it->status == reqError)
				// Recreate it and put it back in the new_queue
				pqueue_push(&new_queries, Connection::retryConnection(it, bannercrawl));
			else if (it->status == reqComplete)
				// Put it in the completed queue
				pqueue_push(&completed_queries, it);
		}
	}
	
	printf("No more things to crawl, waiting for threads...\n");
	pqueue_release(&dns_queries);
	pqueue_release(&curl_queries);
	pqueue_release(&completed_queries);
	
	pthread_join(dbt, NULL);
	if (!bannercrawl) {
		for (int i = 0; i < max_curl_inflight; i++)
			pthread_join (curl_workers[i], NULL);
		pthread_join (dns_worker, NULL);
	}

	delete db;
}

std::pair<std::string, std::string> separate_body(std::string in) {
	std::string body, header;
	auto bodypos = in.find("\r\n\r\n");

	if (bodypos != std::string::npos)
		body = in.substr(bodypos + 4);
	header = in.substr(0, bodypos);

	return std::make_pair(header, body);
}

unsigned parse_hex(const std::string &in) {
	int ret = 0;
	bool valid = true;
	for (unsigned i = 0; i < in.size() && valid; i++) {
		if (in[i] >= '0' && in[i] <= '9')
			ret = (ret << 4) | (in[i] - '0');
		else if (in[i] >= 'a' && in[i] <= 'f')
			ret = (ret << 4) | (in[i] - 'a' + 10);
		else if (in[i] >= 'A' && in[i] <= 'F')
			ret = (ret << 4) | (in[i] - 'A' + 10);
		else
			valid = false;
	}
	return ret;
}

inline size_t casefind(const std::string haystack, const std::string needle) {
	auto it = std::search(
		haystack.begin(), haystack.end(),
		needle.begin(),   needle.end(),
		[](char ch1, char ch2) { return std::toupper(ch1) == std::toupper(ch2); }
	);
	if (it == haystack.end())
		return std::string::npos;
	return it - haystack.begin();
}

std::string parse_response(std::string in, const std::string current_url) {
	// Look for redirects only in the header
	auto hb = separate_body(in);
	auto header = hb.first;

	// Add \n as prefix since it gets confused with Content-Location
	// which results in unnecessary redirections (and maybe other similar headers!)
	auto locpos = casefind(header, "\nLocation:");
	if (locpos == std::string::npos) return "";

	auto location = header.substr(locpos + 9);
	while (location.size() && location[0] == ' ')
		location = location.substr(1);

	std::string loc = location;
	for (unsigned i = 0; i < location.size(); i++) {
		if (location[i] == ' ' || location[i] == '\r') {
			loc = location.substr(0, i);
			break;
		}
	}

	if (loc.substr(0, 7) == "http://" || loc.substr(0, 8) == "https://")
		return loc;
	return base_url(current_url) + loc;
}


// In case of "Transfer-Encoding: chunked" de-chunk the HTTP body
std::string dechunk_http(std::string in) {
	auto hb = separate_body(in);
	auto header = hb.first;
	auto body = hb.second;
	if (body.empty()) return in;
	
	// Look for Transfer-encoding
	auto tfencpos = casefind(header, "Transfer-encoding");
	if (tfencpos == std::string::npos) return in;
	// Look for the "chunked" word before the next break line
	auto tfencposend = header.find("\r\n", tfencpos);
	if (tfencposend == std::string::npos) return in;

	std::string content = header.substr(tfencpos, tfencposend - tfencpos);
	if (casefind(content, "chunked") == std::string::npos) return in;

	// Now proceed to dechunk the body
	std::string ret = header + "\r\n\r\n";
	while ( body.size() ) {
		unsigned len = parse_hex(body);
		if (len + ret.size() > in.size())
			break; // Overflow, should not happen
		if (len == 0)
			break;  // Last chunk
		auto nextp = body.find("\r\n");
		if (nextp == std::string::npos)
			break;

		ret += body.substr(nextp + 2, len);
		body = body.substr(nextp + 2 + len);
	}
	return ret;
}

// DNS callback for c-ares
static void dns_callback(void *arg, int status, int timeouts, struct hostent *host) {
	Connection * c = (Connection*)arg;
	dns_inflight_--;

	if (status == ARES_SUCCESS) {
		uint8_t **p = (uint8_t**)host->h_addr_list;
		if (p && *p) {
			uint8_t ip[4] = { *(*p+0), *(*p+1), *(*p+2), *(*p+3) };
		
			uint32_t lip = (ip[0]<<24) | (ip[1]<<16) | (ip[2]<<8) | (ip[3]);
			c->ip = lip;
			//c->start_time = time(0);
		
			//if (verbose)
			//	std::cout << "Resolved " << tosolve << " to " << ip << std::endl;
		
			pqueue_push(&connection_queries, c);
			return;
		}
	}
	else {
		if (verbose)
			std::cout << "ARES FAILURE " << c->url << " " << status << std::endl;
	}

	// Since cares already retries DNS requests, make sure it doens't respin
	c->retries = MAX_RETRIES;
	pqueue_push(&new_queries, c);
}

// Wait for DNS requests, process them one at a time and store the IP back
void * dns_dispatcher(void * args) {
	// Ares init with options
	ares_channel channel;
	ares_library_init(ARES_LIB_INIT_ALL);
	if (ARES_SUCCESS != ares_library_init(ARES_LIB_INIT_ALL)) {
		fprintf(stderr, "ares_library_init error\n");
		exit(1);
	}

	struct ares_options a_opt;
	memset(&a_opt, 0, sizeof(a_opt));
	a_opt.tries = 1;
	a_opt.timeout = MAX_DNSTIMEOUT;
	if (ARES_SUCCESS != ares_init_options(&channel, &a_opt, ARES_OPT_TRIES | ARES_OPT_ROTATE | ARES_OPT_TIMEOUT)) {
		std::cerr << "Error with c-ares!" << std::endl;
		exit(1);
	}

	// Init servers (mix of IPv4 and IPv6, thus use ares_set_servers
	struct ares_addr_node dns_servers_list[sizeof(dns_servers)/sizeof(dns_servers[0])];
	const unsigned num_servers = sizeof(dns_servers)/sizeof(dns_servers[0]);
	for (unsigned i = 0; i < num_servers; i++) {
		if (strchr(dns_servers[i], '.') == NULL) {
			dns_servers_list[i].family = AF_INET6;
			inet_pton(AF_INET6, dns_servers[i], &dns_servers_list[i].addr.addr6);
		} else {
			dns_servers_list[i].family = AF_INET;
			inet_pton(AF_INET, dns_servers[i], &dns_servers_list[i].addr.addr4);
		}
		dns_servers_list[i].next = &dns_servers_list[i+1];
	}
	dns_servers_list[num_servers-1].next = NULL;

	if (ARES_SUCCESS != ares_set_servers(channel, dns_servers_list)) {
		fprintf(stderr, "ares_set_servers error\n");
		exit(1);
	}
	
	do {
		// Retrieve new queries and add them
		Connection * c = (Connection*)pqueue_pop_nonb(&dns_queries);
		while (c) {
			if (c->status != reqDnsQuery) printf("%d %s\n", (int)c->status, c->url.c_str());
			assert(c->status == reqDnsQuery);
			c->status = reqDnsQuerying;
			std::string tosolve = gethostname(c->url);
			
			ares_gethostbyname(channel, tosolve.c_str(), AF_INET, dns_callback, (void*)c);
			dns_inflight_++;
			c = (Connection*)pqueue_pop_nonb(&dns_queries);
		}

		// Process DNSs
		struct timeval tv;
		fd_set read_fds, write_fds;
		FD_ZERO(&read_fds);
		FD_ZERO(&write_fds);
		int nfds = ares_fds(channel, &read_fds, &write_fds);
		struct timeval * tvp = ares_timeout(channel, NULL, &tv);
		if (nfds > 0)
			select(nfds, &read_fds, &write_fds, NULL, tvp);
		ares_process(channel, &read_fds, &write_fds);
		
		// Sleep while not working
		if (dns_inflight_ == 0)
			pqueue_wait(&dns_queries);
			
		// No queries working and no more to come, exit!
	} while (dns_inflight_ != 0 || !pqueue_released(&dns_queries));

	ares_destroy(channel);
	ares_library_cleanup();

	printf("Exiting DNS thread...\n");

	return 0;
}

// Walk the table from time to time and insert results into the database
void * database_dispatcher(void * args) {
	bool bannercrawl = *(bool*)args;
	char sql_query[BUFSIZE*3];
	unsigned long long num_processed = 0;
	
	Connection * c = (Connection*)pqueue_pop(&completed_queries);
	
	while (c) {
		assert(c->status == reqComplete || c->status == reqCompleteError);

		// Analyze the response, maybe we find some redirects
		if (!c->dechunked)
			c->inbuffer = dechunk_http(c->inbuffer);
		std::string newloc = parse_response(c->inbuffer, c->url);
		
		if (newloc != "" && c->redirs < MAX_REDIRS && !bannercrawl) {
			// Redirect! Setup the query again
			c = Connection::redirConnection(c, newloc);
			int redirs = c->redirs + 1;

			if (verbose)
				std::cout << "Redir " << c->vhost << " >> " << newloc << std::endl;

			pqueue_push(&new_queries, c);
		} else {
			if (bannercrawl) {
				db->updateService(c->ip, c->port, c->inbuffer);
			}else{
				int eflag = c->status == reqCompleteError ? 0x80 : 0;
				auto hbparsed = separate_body(c->inbuffer);

				db->updateVhost(c->vhost, c->url, hbparsed.first, hbparsed.second, eflag);
			}
			num_processed++;
			if (verbose)
				std::cout << "Completed " << c->url << std::endl;

			// Entry cleanup
			delete c;
			num_completed++;  // Allow one more to come in
		}
		
		c = (Connection*)pqueue_pop(&completed_queries);
	}
	
	printf("End of crawl, quitting!\n");
	printf("Inserted %llu registers\n", num_processed);

	return 0;
}

// CURL worker thread
static size_t curl_fwrite(void *buffer, size_t size, size_t nmemb, void *stream) {
	Connection *c = (Connection*)stream;
	
	if (time(0) - c->start_time > MAX_TIMEOUT*10)
		return -1;   // This timeout for slow loading websites

	if (c->inbuffer.size() < BUFSIZE)
		c->inbuffer.append((char*)buffer, size*nmemb);

	return size*nmemb;
}

void * curl_dispatcher(void * args) {
	Connection * c = (Connection*)pqueue_pop(&curl_queries);
	while (c) {
		curl_inflight_++;

		assert(c->status == reqCurl);
		c->status = reqCurlTransfer;
		c->inbuffer.clear();

		CURL * curl = curl_easy_init();
		curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0);
		curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0);
		curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1);
		curl_easy_setopt(curl, CURLOPT_HEADER, 1);
		curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1);
		curl_easy_setopt(curl, CURLOPT_TIMEOUT , MAX_TIMEOUT);
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_fwrite);
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, c);
		curl_easy_setopt(curl, CURLOPT_URL, c->url.c_str());

		if (verbose)
			std::cout << "cURL Fetch " << c->url << std::endl;
		CURLcode res = curl_easy_perform(curl);

		if(CURLE_OK == res) {
			c->status = reqComplete;
			c->dechunked = true;
			pqueue_push(&completed_queries, c);
		}
		else {
			c->status = reqError;
			pqueue_push(&new_queries, c);
		}

		curl_easy_cleanup(curl);
		curl_inflight_--;
		
		// Next request!
		c = (Connection*)pqueue_pop(&curl_queries);
	}
	
	std::cout << "Closing cURL thread" << std::endl;
	return 0;
}

