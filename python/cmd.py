
# Command line version of wwitt
import os
import sys
import time
import itertools
import signal

from pool_scheduler import Pool_Scheduler
from async_http import Async_HTTP
from dns_solver import DNS_Solver
from port_scanner import Port_Scanner
import ip_crawler
from db_interface import DBInterface

def termhand():
	sys.exit(0)
signal.signal(signal.SIGTERM, termhand)


print( "  __      __  __      __  ______  ______  ______" )
print( " /\ \  __/\ \/\ \  __/\ \/\__  _\/\__  _\/\__  _\ " )
print( " \ \ \/\ \ \ \ \ \/\ \ \ \/_/\ \/\/_/\ \/\/_/\ \/ " )
print( "  \ \ \ \ \ \ \ \ \ \ \ \ \ \ \ \   \ \ \   \ \ \ " )
print( "   \ \ \_/ \_\ \ \ \_/ \_\ \ \_\ \__ \ \ \   \ \ \ " )
print( "    \ `\___x___/\ `\___x___/ /\_____\ \ \_\   \ \_\ " )
print( "     '\/__//__/  '\/__//__/  \/_____/  \/_/    \/_/ " )
print( "" )
print( "         World Wide Internet Takeover Tool" )
print( "" )

if len(sys.argv) == 1:
	print( "Usage: " + sys.argv[0] + " option [params]" )
	print( "" )
	print( "Options:" )
	print( " portscan ipfrom ipto [port list]" )
	print( " virtualhosts ipfrom ipto" )
	print( " httpcrawl [re-scan]" )
	sys.exit(1)

option = sys.argv[1]
if option == "portscan":
	ipfrom = sys.argv[2]
	ipto   = sys.argv[3]
	ports  = [ int(x) for x in sys.argv[4:] ]
	if len(ports) == 0: ports = [21,22,25,80,443,8080]
	
	db = DBInterface()
	dnspool = Pool_Scheduler(10,DNS_Solver)
	portscanpool = Pool_Scheduler(4,Port_Scanner,dnspool,db)

	iplist = list(ip_crawler.iterateIPRange(ipfrom,ipto))
	compoud_list = [ (x, ports) for x in iplist ]
	
	# Perfom port scan, limit outstanding jobs (Linux usually limits # of open files to 1K)
	max_jobs = 3500
	batch_size = 50
	print( "Starting ..." )
	try:
		while len(compoud_list) != 0:
			nj = portscanpool.numActiveJobs()
			t = time.time()
			while nj < max_jobs-batch_size and len(compoud_list) != 0 and time.time()-t < 10:
				portscanpool.addWork( compoud_list[:batch_size] )
				compoud_list = compoud_list[batch_size:]
				nj = portscanpool.numActiveJobs()
	
			print( ("%.2f" % ((1-float(len(compoud_list)*len(ports) + nj)/(len(iplist)*len(ports)))*100)), " % completed ..." )
			time.sleep(1)
	except KeyboardInterrupt as e:
		portscanpool.finalize()
		dnspool.finalize()

	portscanpool.wait()
	dnspool.wait()

elif option == "virtualhosts":
	ipfrom = sys.argv[2]
	ipto   = sys.argv[3]

	db = DBInterface()
	dnspool = Pool_Scheduler(10,DNS_Solver)
	httppool = Pool_Scheduler(10,Async_HTTP,dnspool,db,ip_crawler.parseBing)

	iplist = list(ip_crawler.iterateIPRange(ipfrom,ipto))
	dbips = db.select("hosts","ip")
	qip = list(set(iplist) & set(dbips))

	# Create BING http requests job list
	compoud_list = []
	for ip in qip:
		compoud_list += ip_crawler.getHostlist(ip)
	
	# Perfom port scan, limit outstanding jobs (Linux usually limits # of open files to 1K)
	print( "Starting ..." )
	max_jobs = 30
	batch_size = 5
	totalj = len(compoud_list)
	try:
		while len(compoud_list) != 0:
			nj = httppool.numActiveJobs()
			t = time.time()
			while nj < max_jobs-batch_size and len(compoud_list) != 0 and time.time()-t < 10:
				httppool.addWork( compoud_list[:batch_size] )
				compoud_list = compoud_list[batch_size:]
				nj = httppool.numActiveJobs()
	
			print( ("%.2f" % ((1-float(len(compoud_list) + nj)/(totalj))*100)), " % completed ..." )
			time.sleep(1)
	except KeyboardInterrupt as e:
		httppool.finalize()
		dnspool.finalize()

	httppool.wait()
	dnspool.wait()
	
elif option == "httpcrawl":
	rescan = False
	if len(sys.argv) >= 3:
		if sys.argv[2] == "1": rescan = True
	db = DBInterface()
	dnspool = Pool_Scheduler(10,DNS_Solver)
	httppool = Pool_Scheduler(10,Async_HTTP,dnspool,db,ip_crawler.index_query)

	vhosts = list(db.select("virtualhosts","host",{"head":"$NULL$"}))
	compoud_list = [ "http://" + x + "/" for x in vhosts ]
	
	# Perfom port scan, limit outstanding jobs (Linux usually limits # of open files to 1K)
	max_jobs = 3500
	batch_size = 50
	print( "Starting ...", len(compoud_list), "jobs" )
	try:
		while len(compoud_list) != 0:
			nj = httppool.numActiveJobs()
			t = time.time()
			while nj < max_jobs-batch_size and len(compoud_list) != 0 and time.time()-t < 10:
				httppool.addWork( compoud_list[:batch_size] )
				compoud_list = compoud_list[batch_size:]
				nj = httppool.numActiveJobs()
	
			print( ("%.2f" % ((1-float(len(compoud_list) + nj)/(len(vhosts)))*100)), " % completed ..." )
			time.sleep(1)
	except KeyboardInterrupt as e:
		httppool.finalize()
		dnspool.finalize()

	httppool.wait()
	dnspool.wait()


