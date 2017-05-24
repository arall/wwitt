
CC=gcc
CXX=g++

TADNS=-Itadns tadns/tadns.cc tadns/tadns_common.cc -DTADNS_RESOLVER

#DNSLIB=$(TADNS)   # Uncomment to use tadns as dns backend
DNSLIB=-lcares

all:	vhost crawler scan dispatcher_daemon


dispatcher_daemon:
	$(CXX) -o dispatcher_daemon dispatcher_daemon.cc -O0 -ggdb -lpthread -lcurl

scan:
	$(CXX) -o scan scan.cc -lpcap  -lpthread -O2 crawler_kc.cc -lkyotocabinet

host_fill_kc:
	g++ -o host_fill_kc host_fill_kc.cc http.pb.cc util.cc -lkyotocabinet -lbrotlienc -lbrotlidec -lbrotlicommon -lprotobuf

crawler:
	protoc -I=. --cpp_out=. http.proto
	protoc -I=. --python_out=. http.proto
	$(CXX) -o crawler crawler.cc crawler_kc.cc util.cc http.pb.cc $(DNSLIB) -lkyotocabinet -lpthread -O2 -ggdb -lcurl -lbrotlienc -lbrotlidec -lbrotlicommon -lprotobuf
	# `mysql_config --include` `mysql_config --libs`

vhost:
	$(CXX) -o captcha.o -c captcha.cc  -O0 -ggdb
	$(CC) -o vhost.o -c vhost.c `mysql_config --include`  -O0 -ggdb
	$(CXX) -o vhost vhost.o captcha.o  -ltesseract -llept -lpthread -lcurl -ggdb `mysql_config --include` `mysql_config --libs ` -lpcre

clean:
	rm -rf scan crawler vhost dispatcher_daemon *.o

