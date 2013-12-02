
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <pcap.h>
#include <signal.h>
#include <net/ethernet.h>
#include <pthread.h>
#include <semaphore.h>
#include <mysql.h>

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

#define MAX_OUTSTANDING_QUERIES (1024*64)
struct port_query {
	struct in_addr ip;
	unsigned short port;
	unsigned char status; // 0 unused, 1 scanning, 2 open, 3 filtered
	unsigned char checks;
} port_scans[MAX_OUTSTANDING_QUERIES];
unsigned int num_t_ent = 0;
unsigned long long num_ports_open = 0;
unsigned long long num_ports_filtered = 0;


uint16_t checksum_comp(uint16_t *addr, int len);
void process_packet(unsigned char *args, const struct pcap_pkthdr *header, const unsigned char *buffer);
void * database_dispatcher(void * args);
void * query_adder(void * args);
void insert_register(struct in_addr ip, unsigned int port, unsigned int status, char *q1, char *q2);
void inject_packet(char * datagram);
void forge_packet(char * datagram, const struct in_addr * targetip, int targetport);
void mysql_initialize();
void flush_db(const char * q1, const char * q2);
void sql_prepare(char *q1, char *q2);

pthread_mutex_t hash_table_mutex = PTHREAD_MUTEX_INITIALIZER;

pcap_if_t *alldevsp;
pcap_t *handle;
struct sockaddr_in *ipP;  // Local IP
int sockfd;
struct in_addr current_ip;

int maxpp;
int total_ips, total_ports;
int portlist[32];
volatile int adder_finish = 0;

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
	
	if (argc < 5) {
		fprintf(stderr,"Usage: %s IPstart IPend ports{max 32} device\n", argv[0]);
		exit(1);
	}
	
	// Parse some stuff
	{
		total_ports = 0;
		const char * plist = argv[3];
		while (plist) {
			portlist[total_ports++] = atoi(plist);
			plist = strstr(plist, ",");
			if (plist) plist++;
		}
		int KBps = 500;
		maxpp = (double)(KBps)*1024 / 64;  // Aprox formula
		
		struct in_addr last_ip;
		inet_aton(argv[1], &current_ip);
		inet_aton(argv[2], &last_ip);
		total_ips = ntohl(last_ip.s_addr) - ntohl(current_ip.s_addr);
		
		printf("Staring to scan %d ip addresses and %d ports per address\n", total_ips, total_ports);
	}
	
	if( pcap_findalldevs( &alldevsp , errbuf) < 0 ) {
		fprintf(stderr, "findalldevs failed!\n");
		exit(1);
	}

	while (alldevsp != NULL) {
		if (strcmp(argv[4], alldevsp->name) == 0)
			break;
		alldevsp = alldevsp->next;
	}
	if (alldevsp == NULL) {
		fprintf(stderr, "Could not open %d dev for pcap!\n", argv[4]);
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
	
	mysql_initialize();
	
	// Start!
	pthread_t scanner, db;
	pthread_create (&scanner, NULL, &query_adder, NULL);
	pthread_create (&db, NULL, &database_dispatcher, NULL);
	
	signal(SIGTERM, &sigterm);
	signal(SIGINT, &sigterm);
	
	pcap_loop(handle , -1 , process_packet , NULL);
}

MYSQL *mysql_conn;
void mysql_initialize() {
	char *server = getenv("MYSQL_HOST");
	char *user = getenv("MYSQL_USER");
	char *password = getenv("MYSQL_PASS");
	char *database = getenv("MYSQL_DB");
	mysql_conn = mysql_init(NULL);
	/* Connect to database */
	printf("Connecting to mysqldb...\n");
	if (!mysql_real_connect(mysql_conn, server, user, password, database, 0, NULL, 0)) {
		fprintf(stderr, "%s\n", mysql_error(mysql_conn));
		fprintf(stderr, "User %s Pass %s Host %s Database %s\n", user, password, server, database);
		exit(1);
	}
	printf("Connected!\n");
}

void hash_table_add(struct in_addr ip, unsigned short port, unsigned char status) {
	// Create a hash
	unsigned int hash = ( ip.s_addr ^ port ^ (port >> 2) ^ (ip.s_addr >> 7) ^ 0xdeadbeef ) % MAX_OUTSTANDING_QUERIES;
	unsigned int ptr = (hash + 1) % MAX_OUTSTANDING_QUERIES;

	pthread_mutex_lock(&hash_table_mutex);
	while (ptr != hash) {
		if (port_scans[ptr].status == 0) {
			port_scans[ptr].status = status;
			port_scans[ptr].port = port;
			port_scans[ptr].ip = ip;
			port_scans[ptr].checks = 0;
			num_t_ent++;
			break;
		}
		ptr = (ptr + 1) % MAX_OUTSTANDING_QUERIES;
	}
	pthread_mutex_unlock(&hash_table_mutex);
}

void hash_table_del(struct port_query * ent) {
}

struct port_query * hash_table_lookup(struct in_addr ip, unsigned short port) {
	unsigned int hash = ( ip.s_addr ^ port ^ (port >> 2) ^ (ip.s_addr >> 7) ^ 0xdeadbeef ) % MAX_OUTSTANDING_QUERIES;
	unsigned int ptr = (hash + 1) % MAX_OUTSTANDING_QUERIES;
	
	pthread_mutex_lock(&hash_table_mutex);
	while (ptr != hash) {
		if (port_scans[ptr].ip.s_addr == ip.s_addr && port_scans[ptr].port == port && port_scans[ptr].status != 0) {
			pthread_mutex_unlock(&hash_table_mutex);
			return &port_scans[ptr];
		}
		ptr = (ptr + 1) % MAX_OUTSTANDING_QUERIES;
	}
	pthread_mutex_unlock(&hash_table_mutex);
	return NULL;
}

struct in_addr nextip() {
	current_ip.s_addr = htonl(ntohl(current_ip.s_addr)+1);
	return current_ip;
}

// This thread creates packets, accounts them and then sends them through the wire
void * query_adder(void * args) {
	// For each IP, forge a packet for each port
	sleep(1);
	
	int i,j;
	unsigned long long injected_packets = 0;
	unsigned int init_time = time(0);
	for (i = 0; i < total_ips && adder_finish == 0; i++) {
		struct in_addr ip_dst = nextip();
		char packet[32][128];
		memset(packet, 0, sizeof(packet));
		for (j = 0; j < total_ports; j++) {
			int port_dst = portlist[j];
			forge_packet(packet[j], &ip_dst, port_dst);
			hash_table_add(ip_dst, port_dst, 1);
		}
		
		for (j = 0; j < total_ports; j++) {
			inject_packet(packet[j]);
			injected_packets++;
		}
		
		// Throttle & limit stuff!
		while (num_t_ent > MAX_OUTSTANDING_QUERIES - 32)
			sleep(1);
		while (((double)injected_packets)/(double)(time(0)-init_time+1) > maxpp)
			sleep(1);
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
	struct sniff_ip *iph = (struct sniff_ip*)(buffer + sizeof(struct ethhdr));
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
			struct port_query * entry = hash_table_lookup(dest.sin_addr, ntohs(tcph->th_sport));
			//printf("%d port\n",ntohs(tcph->th_sport));
			if (entry != NULL) {
				//printf("Found in table!\n");
				entry->status = (open) ? 2 : 3;
			}
		}
	}
}

// Walk the table from time to time and insert scanned IPs and remove the timed out wans
void * database_dispatcher(void * args) {
	char sql_query[2][64*1024];
	sql_prepare(sql_query[0],sql_query[1]);

	do {
		unsigned int ptr = MAX_OUTSTANDING_QUERIES;
		do {
			ptr--;
			struct port_query * entry = &port_scans[ptr];
			if (entry->status == 2 || entry->status == 3) {
				if (strlen(sql_query[0]) > sizeof(sql_query[0])*0.9f || 
					strlen(sql_query[1]) > sizeof(sql_query[1])*0.9f) {
					
					flush_db(sql_query[0],sql_query[1]);
					sql_prepare(sql_query[0],sql_query[1]);
				}
				insert_register(entry->ip, entry->port, entry->status, sql_query[0],sql_query[1]);
			}
			
			pthread_mutex_lock(&hash_table_mutex);
			if (entry->status)
				entry->checks++;
			if (entry->status == 2 || entry->status == 3) {
				// Save open/filtered
				num_ports_open += (entry->status == 2) ? 1:0;
				num_ports_filtered += (entry->status == 3) ? 1:0;
				
				entry->status = 0;
				num_t_ent--;
			}
			if (entry->status == 1 && entry->checks > 10) {
				// Save open/filtered
				entry->status = 0;
				num_t_ent--;
			}
			pthread_mutex_unlock(&hash_table_mutex);
		} while (ptr > 0);

		sleep(1);
		printf("Outstanding scans: %d\n",num_t_ent);
	} while (!adder_finish || num_t_ent > 0);
	
	flush_db(sql_query[0],sql_query[1]);
	
	printf("End of scan, quitting!\n");
	printf("Open %llu Filtered %llu\n", num_ports_open, num_ports_filtered);

	mysql_close(mysql_conn);
	exit(0);
}

void sql_prepare(char *q1, char *q2) {
	strcpy(q1, "INSERT IGNORE INTO `hosts` (`ip`,`dateAdd`) VALUES ");
	strcpy(q2, "INSERT IGNORE INTO `services` (`ip`, `port`, `status`) VALUES ");
}

void insert_register(struct in_addr ip, unsigned int port, unsigned int status, char *q1, char *q2) {
	char * comma = ",";
	if (q1[strlen(q1)-1] == ' ') comma = " ";
	
	char temp[4096];
	sprintf(temp,"%s('%d',now())", comma, ntohl(ip.s_addr));
	strcat(q1,temp);
	sprintf(temp,"%s('%d', '%d', '%d')", comma, ntohl(ip.s_addr), port, 3-status);
	strcat(q2,temp);
}

void flush_db(const char * q1, const char * q2) {
	mysql_query(mysql_conn, q1);
	mysql_query(mysql_conn, q2);
}


