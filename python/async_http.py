
# Async HTTP requests
# Requests HTTP webs using nonblocking sockets
# It can retrieve more than one web at a time using only ONE thread

import socket
import time
import traceback
import select
import urllib.parse
import errno
from threading import Thread
from threading import Semaphore
from dns_solver import DNS_Solver

# Status: 0,1,2,3 (working) 98,99,100 done (or err)

class Async_HTTP(Thread):

	class web_item:
		def __init__(self,url):
			self._url = url
			self._ourl = url
			self._ip = ""
			self._ip_solver = None
			self._data = ""
			self._socket = None
			self._status = 0
			self._retries = 0
			self._request = ""
			self._response = ""
			self._redir = ""
			self._redirn = 0
			self._time = 0

	# Required Worker method
	def numPendingJobs(self):
		self._queuelock.acquire()
		num = 0
		for web in self._urllist:
			if web._status < 50:
				num += 1
		self._queuelock.release()
		return num

	# Required Worker method
	def enqueueJobs(self,job):
		self.addWebs(job)

	# Required Worker method
	def stopWorking(self):
		self.finalize()

	def waitEnd(self):
		self._exit_on_end = True
		self._waitsem.release()
		self.join()
	
	# URL_list is a list of complete URLs such as http://hst.com:80/path
	def __init__(self,dnspool,db = None, callback = (lambda: None),URL_list = []):
		super(Async_HTTP, self).__init__()

		self._urllist = []
		self._db = db
		self._callback = callback
		for url in URL_list:
			self._urllist.append(Async_HTTP.web_item(url))

		self._waitsem = Semaphore(1)
		self._queuelock = Semaphore(1)
		self._end = False
		self._exit_on_end = False
		self._dns_solver = dnspool

	def run(self):
		self.work()

	def finalize(self):
		self._end = True
		self._waitsem.release()

	def addWebs(self,urls):
		self._queuelock.acquire()
		for url in urls:
			self._urllist.append(Async_HTTP.web_item(url))
		self._queuelock.release()
		self._waitsem.release()

	def getWebIndex(self,web):
		self._queuelock.acquire()
		ret = None
		for url in urls:
			if url._ourl == web:
				ret = url
				break
		self._queuelock.release()
		return ret

	def redirect(self,content,url):
		head = content.split(b"\r\n\r\n")[0]
		ll = head.split(b"\r\n")
		ret = None
		for h in ll:
			if b"Location: " in h[:10]:
				ret = h[10:].strip()
				if not(b"http:" in ret or b"https:" in ret):
					ret = url.encode('utf-8') + ret
				break
		if ret:
			return ret.decode("utf-8",'ignore')
		return None
	
	def work(self):
		# Process all URLS and get their indices
		while not self._end:
			self._queuelock.acquire()  # Lock webs queue

			for web in self._urllist:
				if urllib.parse.urlparse(web._url).scheme == "https": web._status = 98
				if web._status == 0:
					# Resolve IP
					if web._ip == "":
						if web._ip_solver is None:
							web._ip_solver = self._dns_solver.getWorkerInstance()
							web._time = time.time()+20 # Add extra timeout for DNS solving

						ip = web._ip_solver.queryDNS(urllib.parse.urlparse(web._url).hostname)
						if ip is not None:
							web._ip = ip
						else:  # Error at DNS solve, just stop
							web._status = 97
					
					if web._ip != "":
						# Connect and create socket
						if web._socket is not None:
							web._socket.close()
						web._socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
						web._socket.setblocking(0)
						web._status = 1
						web._time = time.time()
				if web._status == 1:
					# Check connection completeness
					try:
						port = urllib.parse.urlparse(web._url).port
						if (port is None): port = 80
						else: port = int(port)

						path = urllib.parse.urlparse(web._url).path
						param = urllib.parse.urlparse(web._url).query
						if param != "": path += "?" + param
						if path is None or path == "": path = "/"

						web._socket.connect((web._ip, port))
						web._status = 2 # Connection OK!
						web._request = "GET " + path + " HTTP/1.0\r\n"
						web._request+= "Host: " + urllib.parse.urlparse(web._url).hostname + "\r\n"
						web._request+= "User-Agent: Mozilla/5.0 (X11; Linux i686; rv:23.0) Gecko/20100101 Firefox/23.0\r\n"
						web._request+= "\r\n"
						web._request = bytes(web._request, "UTF-8")
					except socket.error as e:
						err = e.args[0]
						if err == errno.EAGAIN or err == errno.EINPROGRESS or err == errno.EALREADY:
							# Just try again after some time
							pass
						else:
							web._retries += 1
							if web._retries < 5:
								web._status = 0
							else:
								web._status = 99 # Stop retrying
					except Exception as e:
						web._status = 99 # Stop retrying
				if web._status == 2:
					# Send request
					if len(web._request) == 0:
						web._status = 3
						web._response = b""
					else:
						try:
							sent = web._socket.send(web._request)
							web._request = web._request[sent:]
						except Exception as e:
							err = e.args[0]
							if err == errno.EAGAIN or err == errno.EINPROGRESS:
								# Just try again after some time
								pass
							else:
								web._retries += 1
								if web._retries < 5:
									web._status = 0
								else:
									web._status = 99 # Stop retrying
				if web._status == 3:
					# Read index response
					try:
						read = web._socket.recv(1024*1024)
						if not read:
							# Finish, parse redirects!
							redir = self.redirect(web._response,web._url)
							if redir == web._redir or web._redirn > 10:
								# Loop!! (or 10 steps...)
								web._status = 95
							elif redir is None:
								web._status = 100
								self._callback(web._url, web._ourl, web._response, self._db)
							else:
								web._url = redir
								web._status = 0
								web._ip = ""
								web._time = time.time()
								web._redirn += 1
							web._redir = redir
						else:
							web._response = web._response + read
					except Exception as e:
						err = e.args[0]
						if err == errno.EAGAIN or err == errno.EINPROGRESS:
							# Just try again after some time
							pass
						else:
							web._retries += 1
							if web._retries < 5:
								web._status = 0
							else:
								web._status = 99 # Stop retrying
				
				if time.time()-web._time > 10:
					web._status = 99
				if web._status > 50 and web._socket is not None:
					web._socket.close()
					web._socket = None

			for web in list(self._urllist):
				if web._status > 50:
					self._urllist.remove(web)

			# Select sockets!
			allready = True
			onesocket = False
			plist = select.poll()
			for web in self._urllist:
				if web._socket is not None and web._status < 50:
					if web._status == 3:
						plist.register(web._socket,select.POLLERR|select.POLLIN)
						allready = False
					elif web._status == 1 or web._status == 2:
						plist.register(web._socket,select.POLLERR|select.POLLOUT)
						allready = False
					else:
						plist.register(web._socket,select.POLLERR)
					onesocket = True
				if web._status == 0:
					allready = False

			self._queuelock.release() # Now we could potentially add/remove webs

			if allready and self._exit_on_end: break

			# Do not waiting if all complete or webs in state 0
			if not allready:
				if onesocket: plist.poll(1000)
				else: time.sleep(1)

			# If work is done, wait here for more work
			if allready:
				self._waitsem.acquire()

			# All waiting for DNS, redirs...

		print ("LOG: Exit thread")


