
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
#include <pcap.h>
#include <pcap/sll.h>
#include <signal.h>
#include <net/ethernet.h>
#include <pthread.h>
#include <semaphore.h>
#include <unordered_map>
#include <atomic>

#include "crawler_kc.h"

#define BUFSIZE (1024*1024) //maximum size of any datagram(16 bits the size of identifier)
#define TRUE 1
#define FALSE 0
#define default_low  1
#define default_high 1024

/* IP header */
struct sniff_ip {
        u_char  ip_vhl;                 /* version << 4 | header length >> 2 */
        u_char  ip_tos;                 /* type of service */
        u_short ip_len;                 /* total length */
        u_short ip_id;                  /* identification */
        u_short ip_off;                 /* fragment offset field */
        #define IP_RF 0x8000            /* reserved fragment flag */
        #define IP_DF 0x4000            /* dont fragment flag */
        #define IP_MF 0x2000            /* more fragments flag */
        #define IP_OFFMASK 0x1fff       /* mask for fragmenting bits */
        u_char  ip_ttl;                 /* time to live */
        u_char  ip_p;                   /* protocol */
        u_short ip_sum;                 /* checksum */
        struct  in_addr ip_src,ip_dst;  /* source and dest address */
};
#define IP_HL(ip)               (((ip)->ip_vhl) & 0x0f)
#define IP_V(ip)                (((ip)->ip_vhl) >> 4)

/* TCP header */
typedef u_int tcp_seq;

struct sniff_tcp {
        u_short th_sport;               /* source port */
        u_short th_dport;               /* destination port */
        tcp_seq th_seq;                 /* sequence number */
        tcp_seq th_ack;                 /* acknowledgement number */
        u_char  th_offx2;               /* data offset, rsvd */
#define TH_OFF(th)      (((th)->th_offx2 & 0xf0) >> 4)
        u_char  th_flags;
        #define TH_FIN  0x01
        #define TH_SYN  0x02
        #define TH_RST  0x04
        #define TH_PUSH 0x08
        #define TH_ACK  0x10
        #define TH_URG  0x20
        #define TH_ECE  0x40
        #define TH_CWR  0x80
        #define TH_FLAGS        (TH_FIN|TH_SYN|TH_RST|TH_ACK|TH_URG|TH_ECE|TH_CWR)
        u_short th_win;                 /* window */
        u_short th_sum;                 /* checksum */
        u_short th_urp;                 /* urgent pointer */
};

/* pseudo header used for tcp checksuming
 * a not so well documented fact ... in public
*/

struct pseudo_hdr {
	u_int32_t src;
	u_int32_t dst;
	u_char mbz;
	u_char proto;
	u_int16_t len;
};

enum pStatus { statusUnknown, statusOpen, statusFiltered };

#define MAX_TIMEOUT 10    // Max 255!
#define MAX_OUTSTANDING_QUERIES (1024*1024*2)
struct port_query {
	struct in_addr ip;
	unsigned short port;
	pStatus status;
	time_t scan_start;
	port_query * next;
};

port_query port_scans[MAX_OUTSTANDING_QUERIES];
port_query *free_list = &port_scans[0];
port_query *timeout_list = NULL, *timeout_list_tail = NULL;
std::unordered_map <uint64_t, port_query*> mapped_scans;

std::atomic<unsigned> num_t_ent(0);
std::atomic<uint64_t> injected_packets(0);

int ethdev;

uint16_t checksum_comp(uint16_t *addr, int len);
void process_packet(unsigned char *args, const struct pcap_pkthdr *header, const unsigned char *buffer);
void * database_dispatcher(void * args);
void * query_adder(void * args);
void insert_register(struct in_addr ip, unsigned int port, unsigned int status, char *q1, char *q2);
void inject_packet(char * datagram);
void forge_packet(char * datagram, const struct in_addr * targetip, int targetport);

pthread_mutex_t hash_table_mutex = PTHREAD_MUTEX_INITIALIZER;

pcap_if_t *alldevsp;
pcap_t *handle;
struct sockaddr_in *ipP;  // Local IP
int sockfd;
struct in_addr current_ip;

int maxpp;
unsigned int total_ips, total_ports;
int portlist[32];
volatile int adder_finish = 0;
DBIface *db = NULL;

void sigterm(int s) {
	printf("Stopping due to SIGINT/SIGTERM... This will take some seconds, be patient :)\n");
	adder_finish = 1;
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
"                    Port scanner                    \n"  );

	printf("Using %d MB \n", sizeof(port_scans)/1024/1024);
	if (argc < 7) {
		fprintf(stderr,"Usage: %s IPstart IPend ports{max 32} speed(mbps) device output.db\n", argv[0]);
		exit(1);
	}

	// Intialize scan structures
	for (unsigned i = 1; i < MAX_OUTSTANDING_QUERIES; i++)
		port_scans[i-1].next = &port_scans[i];

	db = new DBKC(true, argv[6]);
	
	// Parse some stuff
	{
		total_ports = 0;
		const char * plist = argv[3];
		while (plist) {
			portlist[total_ports++] = atoi(plist);
			plist = strstr(plist, ",");
			if (plist) plist++;
		}
		int KBps = (1024 / 8) * atof(argv[4]);
		maxpp = (double)(KBps) * 1024 / 64;  // Aprox formula
		
		struct in_addr last_ip;
		inet_aton(argv[1], &current_ip);
		inet_aton(argv[2], &last_ip);
		total_ips = ntohl(last_ip.s_addr) - ntohl(current_ip.s_addr);
		
		printf("Staring to scan %u ip addresses and %d ports per address at %d KBps (%d pps)\n", total_ips, total_ports, KBps, maxpp);
	}
	
	if( pcap_findalldevs( &alldevsp , errbuf) < 0 ) {
		fprintf(stderr, "findalldevs failed!\n");
		exit(1);
	}

	while (alldevsp != NULL) {
		if (strcmp(argv[5], alldevsp->name) == 0)
			break;
		alldevsp = alldevsp->next;
	}
	if (alldevsp == NULL) {
		fprintf(stderr, "Could not open %s dev for pcap!\n", argv[5]);
		exit(1);
	}
	
	ipP = (struct sockaddr_in *)alldevsp->addresses->next->addr;
	
	if ((handle = pcap_open_live (alldevsp->name, BUFSIZE, 0, 0, errbuf)) == NULL) {
		fprintf (stderr, "Could not open device %s: error: %s \n ", alldevsp->name, errbuf);
		exit (EXIT_FAILURE);
	}
	printf("Successfully opened %s\n",alldevsp->name);
	
	/* Prepare the socket to send raw packets */
	sockfd = socket(AF_INET, SOCK_RAW, IPPROTO_TCP);
	if (sockfd < 0) {
		perror("sock:");
		exit(EXIT_FAILURE);
	}
	
	// Start!
	pthread_t scanner, db;
	pthread_create (&scanner, NULL, &query_adder, NULL);
	pthread_create (&db, NULL, &database_dispatcher, NULL);
	
	signal(SIGTERM, &sigterm);
	signal(SIGINT, &sigterm);

	int devtype = pcap_datalink(handle);
	if (devtype != DLT_EN10MB &&
		devtype != DLT_LINUX_SLL) {
		fprintf (stderr, "Cannot capture on device type %d, only ethernet and SLL supported\n", devtype);
	}

	ethdev = (devtype == DLT_EN10MB);
	printf("Link type: %d\n",devtype);
	
	pcap_loop(handle , -1 , process_packet , NULL);
}

void hash_table_add(struct in_addr ip, unsigned short port) {
	num_t_ent++;

	free_list->status = statusUnknown;
	free_list->port = port;
	free_list->ip = ip;
	free_list->scan_start = time(0);

	pthread_mutex_lock(&hash_table_mutex);
	if (timeout_list_tail) {
		// Merge the two lists and then move ptrs and null terminate it
		timeout_list_tail->next = free_list;
		timeout_list_tail = timeout_list_tail->next;
		free_list = free_list->next;
		timeout_list_tail->next = NULL;
	} else {
		timeout_list = free_list;
		timeout_list_tail = free_list;
		free_list = free_list->next;
	}
	uint64_t key = ip.s_addr | (((uint64_t)port) << 32);
	mapped_scans[key] = timeout_list_tail;
	pthread_mutex_unlock(&hash_table_mutex);
}

void hash_table_del(struct port_query * ent) {
	uint64_t key = ent->ip.s_addr | (((uint64_t)ent->port) << 32);
	mapped_scans.erase(key);
}

port_query * hash_table_lookup(struct in_addr ip, unsigned short port) {
	port_query * ret = NULL;

	uint64_t key = ip.s_addr | (((uint64_t)port) << 32);

	pthread_mutex_lock(&hash_table_mutex);
	if (mapped_scans.count(key))
		ret = mapped_scans.at(key);
	pthread_mutex_unlock(&hash_table_mutex);

	return ret;
}

struct in_addr nextip() {
	current_ip.s_addr = htonl(ntohl(current_ip.s_addr)+1);
	return current_ip;
}

// This thread creates packets, accounts them and then sends them through the wire
void * query_adder(void * args) {
	// For each IP, forge a packet for each port

	int i,j;
	unsigned int init_time = time(0);
	for (i = 0; i < total_ips && adder_finish == 0; i++) {
		struct in_addr ip_dst = nextip();
		char packet[32][128];
		memset(packet, 0, sizeof(packet));
		for (j = 0; j < total_ports; j++) {
			int port_dst = portlist[j];
			forge_packet(packet[j], &ip_dst, port_dst);
			hash_table_add(ip_dst, port_dst);
		}
		
		for (j = 0; j < total_ports; j++) {
			inject_packet(packet[j]);
			injected_packets++;
		}
		
		// Throttle & limit stuff!
		//printf("PPS %f %d \n", ((double)injected_packets)/(double)(time(0)-init_time+1), (unsigned)num_t_ent);
		while (num_t_ent > MAX_OUTSTANDING_QUERIES - 32) {
			sleep(1);
			//printf("SLEEP 1 %d\n", (unsigned)num_t_ent);
		}
		while (((double)injected_packets)/(double)(time(0)-init_time+1) > maxpp) {
			sleep(1);
			//printf("SLEEP 2 %f %d\n", ((double)injected_packets)/(double)(time(0)-init_time+1), maxpp);
		}
	}
	printf("All packets injected :D work done!\n");
	adder_finish = 1;
}

uint16_t checksum_comp (uint16_t *addr, int len) {   /*  compute TCP header checksum */
	register long sum = 0;
	int count = len;
	uint16_t temp;

	while (count > 1)  {
		temp = htons(*addr++);   // in this line:added -> htons
		sum += temp;
		count -= 2;
	}

	/*  Add left-over byte, if any */
	if(count > 0)
		sum += *(unsigned char *)addr;

	/*  Fold 32-bit sum to 16 bits */
	while (sum >> 16)
		sum = (sum & 0xffff) + (sum >> 16);

	uint16_t checksum = ~sum;
	return checksum;
}

void forge_packet(char * datagram, const struct in_addr * targetip, int targetport) {
	int sockfd;
	int timeout = 0;                 /* check if timeout with return from dispatch */
	char temp_addr[16];

	struct sockaddr_in sin;
	sin.sin_family = AF_INET;
	sin.sin_addr = *targetip;
	//inet_pton(AF_INET, targetip, &sin.sin_addr);

	// This is the actual packet buffer.
	struct sniff_ip *iph = (struct sniff_ip *)datagram;
	struct sniff_tcp *tcph = (struct sniff_tcp *)(datagram + sizeof(struct sniff_ip));
	
	iph->ip_vhl = 0x45;                          /* version=4,header_length=5 (no data) */
	iph->ip_tos = 0;                             /* type of service -not needed */
	iph->ip_len = sizeof (struct sniff_ip) + sizeof (struct sniff_tcp);    /* no payload */
	iph->ip_id = htonl(54321);                   /* simple id */
	iph->ip_off = 0;                             /* no fragmentation */
	iph->ip_ttl = 255;                           /* time to live - set max value */
	iph->ip_p = IPPROTO_TCP;                     /* 6 as a value - see /etc/protocols/ */
	iph->ip_src.s_addr = ipP->sin_addr.s_addr;   /*local device IP */
	iph->ip_dst.s_addr = sin.sin_addr.s_addr;    /* dest addr */
	iph->ip_sum = 				     /* no need for ip sum actually */
	checksum_comp( (unsigned short *)iph,
			sizeof(struct sniff_ip));

	tcph->th_sport = htons(1234);                /* arbitrary port */
	tcph->th_dport = htons(targetport);          /* scanned dest port */
	tcph->th_seq = random();                     /* the random SYN sequence */
	tcph->th_ack = 0;                            /* no ACK needed */
	tcph->th_offx2 = 0x50;                       /* 50h (5 offset) ( 8 0s reserverd )*/
	tcph->th_flags = TH_SYN;                     /* initial connection request */
	tcph->th_win = (65535);                      /* maximum allowed window size */
	tcph->th_sum = 0;                            /* will compute later */
	tcph->th_urp = 0;                            /* no urgent pointer */

	/* pseudo header for tcp checksum */
	struct pseudo_hdr *phdr = (struct pseudo_hdr *) (datagram +
			sizeof(struct sniff_ip) + sizeof(struct sniff_tcp));
	phdr->src = iph->ip_src.s_addr;
	phdr->dst = iph->ip_dst.s_addr;
	phdr->mbz = 0;
	phdr->proto = IPPROTO_TCP;
	phdr->len = ntohs(0x14);       /* in bytes the tcp segment length */
	/*- WhyTF is it network byte saved by default ????*/

	tcph->th_sum = htons(checksum_comp((unsigned short *)tcph,
				sizeof(struct pseudo_hdr)+
				sizeof(struct sniff_tcp)));
}

void inject_packet(char * datagram) {
	
	struct sniff_ip *iph = (struct sniff_ip *)datagram;
	struct sniff_tcp *tcph = (struct sniff_tcp *)(datagram + sizeof(struct sniff_ip));
		
	int one = 1;
	const int *val = &one;
	if (setsockopt(sockfd, IPPROTO_IP, IP_HDRINCL, val, sizeof (one)) < 0)
		fprintf(stderr, "Warning: Cannot set HDRINCL\n");

	struct sockaddr_in sin;
	sin.sin_family = AF_INET;
	sin.sin_port = tcph->th_dport;
	sin.sin_addr = iph->ip_dst;

	if (sendto(sockfd, datagram, iph->ip_len, 0, (struct sockaddr *)&sin, sizeof(sin)) < 0) {
		perror("sendto:");
		fprintf(stderr, "Error sending datagram!\n");
	}
}

void process_packet(unsigned char *args, const struct pcap_pkthdr *header, const unsigned char *buffer) {
	// Parse IP and TCP headers. Look for SYN/ACK transactions or any other kind of status
	struct sniff_ip *iph = (ethdev) ? (struct sniff_ip*)(buffer + sizeof(struct ethhdr)) : (struct sniff_ip*)(buffer + sizeof(struct sll_header));
	struct sniff_tcp *tcph = (struct sniff_tcp *)((char*)iph + sizeof(struct sniff_ip));
	struct sockaddr_in dest;
	memset(&dest, 0, sizeof(dest));
	dest.sin_addr.s_addr = iph->ip_src.s_addr;

	char ipv4[128];
	if (iph->ip_p == IPPROTO_TCP && iph->ip_vhl == 0x45) {
		// Look for SYN/ACK or RST
		int open = (tcph->th_flags&TH_SYN) != 0 && (tcph->th_flags&TH_ACK) != 0;
		int filtered = (tcph->th_flags&TH_RST) != 0;
		
		if (open || filtered) {
			// Look up for the packet and then mark the appropriate status
			port_query * entry = hash_table_lookup(dest.sin_addr, ntohs(tcph->th_sport));
			if (entry != NULL) {
				entry->status = (open) ? statusOpen : statusFiltered;
				// FIXME: Optimize the path and move this out of the timeout list
			}
		}
	}
}

// Walk the table from time to time and insert scanned IPs and remove the timed out wans
void * database_dispatcher(void * args) {
	uint64_t num_ports_open = 0;
	uint64_t num_ports_filtered = 0;

	do {
		time_t currtime = time(0);

		pthread_mutex_lock(&hash_table_mutex);
		while (timeout_list != NULL && timeout_list->scan_start + MAX_TIMEOUT < currtime) {
			// Process the entry
			if (timeout_list->status != statusUnknown) {
				// Save open/filtered
				//db->updateService(ntohl(timeout_list->ip.s_addr), timeout_list->port,
				//                  std::to_string((unsigned)timeout_list->status));
				if (timeout_list->status == statusOpen)
					db->addService(ntohl(timeout_list->ip.s_addr), timeout_list->port);

				num_ports_open += (timeout_list->status == statusOpen) ? 1 : 0;
				num_ports_filtered += (timeout_list->status == statusFiltered) ? 1 : 0;
			}

			// Delete entry
			hash_table_del(timeout_list);

			auto elem = timeout_list;
			// Advance the timeout_list
			timeout_list = timeout_list->next;
			if (!timeout_list)
				timeout_list_tail = NULL;

			// Add entry to free_list
			elem->next = free_list;
			free_list = elem;

			num_t_ent--;
		}
		pthread_mutex_unlock(&hash_table_mutex);

		sleep(1);
		int p = 100.0f * injected_packets / (total_ips * total_ports);
		printf("\rOutstanding scans: %06d (%d%% done, %llu)        ", (unsigned)num_t_ent, p, (uint64_t)injected_packets);
		fflush(stdout);
	} while (!adder_finish || num_t_ent > 0);

	delete db;
	
	printf("End of scan, quitting!\n");
	printf("Open %llu Filtered %llu\n", num_ports_open, num_ports_filtered);

	exit(0);
}

