
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
compoud_list = [ (x, portlist) for x in iplist ]

# Perfom port scan, limit outstanding jobs (Linux usually limits # of open files to 1K)
max_jobs = 900
batch_size = 20
while len(iplist) != 0:
	nj = portscanpool.numActiveJobs()
	while nj < max_jobs-batch_size:
		portscanpool.addWork( compoud_list[:batch_size] )
		compoud_list = compoud_list[batch_size:]
		nj = portscanpool.numActiveJobs()
	
	print "Port scanning... ",nj, len(compoud_list)
	time.sleep(1)

portscanpool.wait()

