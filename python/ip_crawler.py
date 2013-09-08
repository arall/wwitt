
# IP Crawler
# Crawls a range of IPs and performs port scanning

import urllib

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
		search = urllib.urlencode(search)
		url = 'http://www.bing.com/search?q=' + search + "&first=" + str(n);
		ret.append(url)
	return ret

# Parses Bing page to get the virtualhosts for a given IP
def parseBing():
	pass

