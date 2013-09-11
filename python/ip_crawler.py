
# IP Crawler
# Crawls a range of IPs and performs port scanning

import urllib
import datetime
import re
import urlparse

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
		search = urllib.urlencode({'q':search, 'first':n})
		url = 'http://www.bing.com/search?' + search
		ret.append(url)
	return ret

# Parses Bing page to get the virtualhosts for a given IP
def parseBing(url,ourl,body,db):
	if "no_results" in body:
		# Do nothing!
		return

	ip = re.findall("IP%3A\+([0-9]*\.[0-9]*\.[0-9]*\.[0-9]*)",url)[0]
	
	# Lazy match (match as many as possible)
	regexp = '<div.*?class="sb_meta".*?>.*?<cite>(.*?)</cite>'
	matches = re.findall(regexp,body)

	# Now get hostname for each one and put into database
	hosts = [ urlparse.urlparse("http://"+x).hostname for x in matches ]

	ipid = list(db.select("hosts","id",{"ip":ip}))[0]

	for h in hosts:
		print( "Virtualhost:",h)
		db.insert("virtualhosts",{'ipId':ipid, 'host':h, 'dateAdd':str(datetime.datetime.now())})
	
def index_query(url,ourl,body,db):
	print( "Got index",url)
	host = urlparse.urlparse(ourl).hostname
	body_head = body.split("\r\n\r\n",1)[0]
	body_body = body.split("\r\n\r\n",1)[1]
	db.update("virtualhosts",{'host':host},{"head":body_head, "index":body_body, "url":url})



