
import urllist

import time

from pool_scheduler import Pool_Scheduler
from async_http import Async_HTTP
from dns_solver import DNS_Solver

# Main test
shortlist = ["http://facebook.com", "http://google.com", "http://youtube.com", "http://yahoo.com", "http://baidu.com"]

dnspool = Pool_Scheduler(10,DNS_Solver)

pool = Pool_Scheduler(10,Async_HTTP,dnspool)
pool.addWork(urllist.wlist)

while True:
	time.sleep(2)
	print "---> ", pool.numActiveJobs()
	
	if pool.numActiveJobs() == 0:
		pool.wait()
		dnspool.wait()
		break


pool.wait()

#test = Async_HTTP(wlist)
#test.start()

#for web in test._urllist:
#	print web._response


