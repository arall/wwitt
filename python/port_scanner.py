
# Async Port Scanner
# Scans a list of ports for given IPs

import socket
import select
import time
import datetime
import errno
from threading import Thread
from threading import Semaphore

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
				self._ports.append([p,None,"",0,0,False,b""])

	# Required Worker method
	def numPendingJobs(self):
		self._queuelock.acquire()
		num = 0
		for target in self._scanlist:
			for port in target._ports:
				if port[2] == "" or port[1] is not None:
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
		self._banner_timeout = 10     # BANNER TIMEOUT
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
			plist = select.poll()
			for target in self._scanlist:
				for porttuple in target._ports:
					if porttuple[2] != "" and porttuple[1] is None: continue
					
					if porttuple[2] == "open":
						# Query the banner!
						endc = False
						plist.register(porttuple[1],select.POLLERR|select.POLLIN)
						try:
							read = porttuple[1].recv(1024*1024)
							if not read:
								porttuple[2] = "read"
								endc = True
							else:
								porttuple[6] = porttuple[6] + read
						except:
							if err != errno.EAGAIN and err != errno.EINPROGRESS:
								endc = True
						if time.time() - porttuple[4] > self._banner_timeout:
							endc = True
						
						if endc:
							porttuple[1].close()
							porttuple[1] = None
							porttuple[2] = "read"
						continue
						
					if porttuple[2] in ["read", "closed"]:
						if porttuple[1] is not None:
							porttuple[1].close()
							porttuple[1] = None
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
						plist.register(porttuple[1],select.POLLERR|select.POLLIN)
					except socket.error as e:
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
						plist.register(porttuple[1],select.POLLERR|select.POLLOUT)

			if self._db is not None:
				self.updateDB()
			
			self._queuelock.release()
			
			if not allready: plist.poll(1000)
			
			if allready and self._exit_on_end: break
			
			if allready:
				self._waitsem.acquire()

		print ("LOG: Exit thread")

	def updateDB(self):
		da = str(datetime.datetime.now())
		for target in self._scanlist:
			# All ports scanned
			alldone = True
			allclosed = True
			for porttuple in target._ports:
				if porttuple[1] is not None or porttuple[2] == "":
					alldone = False
				if porttuple[2] in ["open","read"]:
					allclosed = False
					
			# When all ports for a given IP are done, insert results
			if not target._db and target._ip != "" and alldone:
				st = "0" if allclosed else "1"
				ipi = {"ip": target._ip, "dateAdd":da, "dateUpdate":da, "status": st}
				target._ipid = self._db.insert("hosts",ipi)
				target._db = True
				for porttuple in target._ports:
					if not porttuple[5] and porttuple[2] in ["open","read"] and porttuple[1] is None:
						porti = {"port": porttuple[0],"ipId": target._ipid, "dateAdd":da}
						if porttuple[2] == "read":
							porti["head"] = porttuple[6]
						self._db.insert("services",porti)
						porttuple[5] = True
		# Remove done stuff
		for target in list(self._scanlist):
			if target._db:
				self._scanlist.remove(target)
		



