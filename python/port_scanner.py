
# Async Port Scanner
# Scans a list of ports for given IPs

import socket
import select
import time
import errno
from threading import Thread
from threading import Semaphore

def selectwrapper(read, write, erro, timeout):
	p = select.poll()
	alls = list(set(read)|set(write)|set(erro))
	for fd in alls:
		mask = 0
		if fd in read: mask |= select.POLLIN
		if fd in write: mask |= select.POLLOUT
		if fd in erro: mask |= select.POLLERR
		p.register(fd,mask)
	p.poll(timeout*1000)

class Port_Scanner(Thread):

	class ip_item:
		def __init__(self,host,ports,dns):
			self._host = host
			self._ip = ""
			self._ports = []
			self._db = False
			self._dns_solver = dns
			for p in ports:
				# Port, Socket, Status, Retries, DB
				self._ports.append([p,None,"",0,0,False])

	# Required Worker method
	def numPendingJobs(self):
		self._queuelock.acquire()
		num = 0
		for target in self._scanlist:
			for port in target._ports:
				if port[2] == "":
					num += 1
		self._queuelock.release()
		return num

	# Required Worker method
	def enqueueJobs(self,job):
		self.addScan(job)

	# Required Worker method
	def stopWorking(self):
		self.finalize()

	def waitEnd(self):
		self._exit_on_end = True
		self._waitsem.release()
		self.join()
	
	# URL_list is a list of complete URLs such as http://hst.com:80/path
	def __init__(self, dns_pool, dbi = None):
		super(Port_Scanner, self).__init__()
		
		self._scanlist = []
		self._dnspool = dns_pool
		self._db = dbi

		self._waitsem = Semaphore(1)
		self._queuelock = Semaphore(1)
		self._end = False
		self._exit_on_end = False
		
		self._connection_timeout = 5  # CONNECTION TIMEOUT
		self._connection_retries = 2  # RETRIES

	def run(self):
		self.work()

	def finalize(self):
		self._end = True
		self._waitsem.release()

	# Each element is a (target, [portlist])
	def addScan(self,hostlist):
		self._queuelock.acquire()
		
		for target in hostlist:
			dns = self._dnspool.getWorkerInstance()
			self._scanlist.append(Port_Scanner.ip_item(target[0],target[1],dns))
			
		self._queuelock.release()
		self._waitsem.release()

	def work(self):
		# Process targets and their ports
		while not self._end:
			allready = True

			self._queuelock.acquire()
			socketlist = []
			for target in self._scanlist:
				for porttuple in target._ports:
					if porttuple[2] != "":
						if porttuple[1] is not None:
							porttuple[1].close()
						continue
					
					allready = False
					if target._ip == "":
						target._ip = target._dns_solver.queryDNS(target._host)
						if target._ip is None:
							porttuple[2] = "error"
							continue
						if target._ip == "": continue
					
					host = target._ip
					port = porttuple[0]
					
					# Scan host, port
					if porttuple[1] is None:
						porttuple[1] = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
						porttuple[1].setblocking(0)
						porttuple[4] = time.time()
					# Connect
					try:
						porttuple[1].connect((host, port))
						porttuple[2] = "open"
					except socket.error, e:
						err = e.args[0]
						cerr = False
						if err == errno.EAGAIN or err == errno.EINPROGRESS or err == errno.EALREADY:
							# Just try again after some time
							if time.time() - porttuple[4] > self._connection_timeout:
								cerr = True
						else:
							cerr = True
						
						if cerr:
							porttuple[3] += 1
							if porttuple[3] >= self._connection_retries:
								porttuple[2] = "closed"
								
					if porttuple[2] == "":
						socketlist.append(porttuple[1])

			if self._db is not None:
				self.updateDB()
			
			self._queuelock.release()
			
			if not allready: selectwrapper([],socketlist,socketlist,1)
			
			if allready and self._exit_on_end: break
			
			if allready:
				self._waitsem.acquire()

		print "LOG: Exit thread"

	def updateDB(self):
		for target in self._scanlist:
			if not target._db and target._ip != "":
				ipi = {"ip": target._ip}
				target._ipid = self._db.insert("hosts",ipi)
				target._db = True
			for porttuple in target._ports:
				if not porttuple[5] and porttuple[2] == "open":
					porti = {"port": porttuple[0],"ipId": target._ipid}
					self._db.insert("services",porti)
					porttuple[5] = True



