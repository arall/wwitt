
# IP Crawler
# Crawls a range of IPs and performs port scanning

import urllib
import datetime
import re
import urllib.parse

# Generates ip iteration from ip range
def iterateIPRange(ipfrom,ipto):
	ipfrom = ipfrom.split(".")
	ipto = ipto.split(".")
	ipfrom = [ int(x) for x in ipfrom ]
	ipto = [ int(x) for x in ipto ]
	
	while ipfrom <= ipto:
		yield ".".join((str(x) for x in ipfrom))
		ipfrom[-1] += 1
		for i in reversed(range(len(ipfrom))):
			if ipfrom[i] >= 256:
				ipfrom[i-1] += 1
				ipfrom[i] = 0


# Gets the URL list to query hosts for a given IP
def getHostlist(ip):
	ret = []
	for n in range(0,50,10):
		search = "IP: " + str(ip)
		search = urllib.parse.urlencode({'q':search, 'first':n})
		url = 'http://www.bing.com/search?' + search
		ret.append(url)
	return ret

# Parses Bing page to get the virtualhosts for a given IP
def parseBing(url,ourl,body,db):
	if b"no_results" in body:
		# Do nothing!
		return

	try:
		ip = re.findall("IP%3A\+([0-9]*\.[0-9]*\.[0-9]*\.[0-9]*)",url)[0]
	
		# Lazy match (match as many as possible)
		regexp = b'<div.*?class="sb_meta".*?>.*?<cite>(.*?)</cite>'
		matches = re.findall(regexp,body)

		# Now get hostname for each one and put into database
		hosts = [ urllib.parse.urlparse("http://"+x.decode("utf-8",errors='ignore')).hostname for x in matches ]

		ipid = list(db.select("hosts","id",{"ip":ip}))[0]

		for h in hosts:
			print( "Virtualhost:",h,"for ip:",ip)
			db.insert("virtualhosts",{'ipId':ipid, 'host':h, 'dateAdd':str(datetime.datetime.now())})
	except Exception as e:
		print(e)
	
def index_query(url,ourl,body,db):
	try:
		host = urllib.parse.urlparse(ourl).hostname
		body_head = body.split(b"\r\n\r\n",1)[0]
		body_body = body.split(b"\r\n\r\n",1)[1]
		print ("Got index",ourl)
	except Exception as e:
		print (e)
	db.update("virtualhosts",{'host':host},{"head":body_head, "index":body_body, "url":url})



