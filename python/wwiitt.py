
import curses
import urllist
import time
import itertools

from pool_scheduler import Pool_Scheduler
from async_http import Async_HTTP
from dns_solver import DNS_Solver
from port_scanner import Port_Scanner
import ip_crawler

def ipscan(ipfrom,ipto,log,info,screen):
	dnspool = Pool_Scheduler(10,DNS_Solver)
	#pool = Pool_Scheduler(4,Async_HTTP,dnspool)
	portscanpool = Pool_Scheduler(4,Port_Scanner,dnspool)

	# Generate IP range, and get VirtualHosts
	iplist = list(ip_crawler.iterateIPRange(ipfrom,ipto))
	portlist = [21,22,25,80,443,8080]
	compoud_list = [ (x, portlist) for x in iplist ]
	
	# Prepare UI
	curses.cbreak()
	screen.nodelay(True);

	# Perfom port scan, limit outstanding jobs (Linux usually limits # of open files to 1K)
	max_jobs = 3500 #800
	batch_size = 50 #20
	while len(compoud_list) != 0:
		nj = portscanpool.numActiveJobs()
		while nj < max_jobs-batch_size and len(compoud_list) != 0:
			portscanpool.addWork( compoud_list[:batch_size] )
			compoud_list = compoud_list[batch_size:]
			nj = portscanpool.numActiveJobs()
	
		complete = "%.2f" % ((1-float(len(compoud_list)*len(portlist) + nj)/(len(iplist)*len(portlist)))*100)
		info.addstr(16,5,str(complete) + " % done ...    ")
		info.border()
		info.redrawwin()
		info.refresh()

		time.sleep(1)

	portscanpool.wait()
	dnspool.wait()
	
	curses.nocbreak()
	screen.nodelay(False);


