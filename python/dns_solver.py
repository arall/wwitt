
# Async DNS solver
# Solves DNS by using a thread and blocking

import socket
import select
from threading import Thread
from threading import Semaphore

# Status: 0,1,2,3 (working) 98,99,100 done (or err)

class DNS_Solver(Thread):

	class dns_item:
		def __init__(self,host):
			self._host = host
			self._ip = ""

	# Required Worker method
	def numPendingJobs(self):
		self._queuelock.acquire()
		num = 0
		for dns in self._dnslist:
			if dns._ip != "":
				num += 1
		self._queuelock.release()
		return num

	# Required Worker method
	def enqueueJobs(self,job):
		self.addDNS(job)

	# Required Worker method
	def stopWorking(self):
		self.finalize()

	def waitEnd(self):
		self._exit_on_end = True
		self._waitsem.release()
		self.join()
	
	# URL_list is a list of complete URLs such as http://hst.com:80/path
	def __init__(self,hostlist = []):
		super(DNS_Solver, self).__init__()

		self._dnslist = []
		for dns in hostlist:
			self._dnslist.append(DNS_Solver.dns_item(dns))

		self._waitsem = Semaphore(1)
		self._queuelock = Semaphore(1)
		self._end = False
		self._exit_on_end = False

	def run(self):
		self.work()

	def finalize(self):
		self._end = True
		self._waitsem.release()

	def addDNS(self,dnss):
		self._queuelock.acquire()
		for dns in dnss:
			self._dnslist.append(DNS_Solver.dns_item(dns))
		self._queuelock.release()
		self._waitsem.release()

	def queryDNS(self,host):
		self._queuelock.acquire()
		ret = None
		for dns in self._dnslist:
			if dns._host == host:
				ret = dns._ip
				break
		self._queuelock.release()

		if ret is None:
			self.addDNS([host])

		return ret
	
	def work(self):
		# Process all URLS and get their indices
		while not self._end:
			allready = True
			print "dp2"

			self._queuelock.acquire()
			for dns in self._dnslist:
				if dns._ip == "": # Query it!
					host = dns._host
					allready = False
					break
			self._queuelock.release()

			if not allready:
				try:
					print "querying ", host
					ip = socket.gethostbyname(host)
				except:
					ip = None

				self._queuelock.acquire()  # Lock webs queue
				for dns in self._dnslist:
					if dns._host == host:
						dns._ip = ip
						break
				self._queuelock.release()

			if allready and self._exit_on_end: break

			if allready:
				self._waitsem.acquire()

		print "LOG: Exit thread"


