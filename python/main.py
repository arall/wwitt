
import urllist
import time
import itertools

from pool_scheduler import Pool_Scheduler
from async_http import Async_HTTP
from dns_solver import DNS_Solver
from port_scanner import Port_Scanner
import ip_crawler

# Main test
dnspool = Pool_Scheduler(10,DNS_Solver)
pool = Pool_Scheduler(4,Async_HTTP,dnspool)
portscanpool = Pool_Scheduler(4,Port_Scanner,dnspool)

# Generate IP range, and get VirtualHosts
iplist = list(ip_crawler.iterateIPRange("5.135.0.0","5.135.255.255"))
portlist = [21,22,25,80,443,8080]

# Perfom port scan, limit outstanding jobs (Linux usually limits # of open files to 1K)
while len(iplist) != 0:
	while portscanpool.numActiveJobs() < 950:
		portscanpool.addWork( [ (iplist.pop(), portlist) ] )
	
	print "Port scanning... ",portscanpool.numActiveJobs(), len(iplist)
	time.sleep(1)

portscanpool.wait()

