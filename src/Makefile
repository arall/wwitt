
CC=gcc
CXX=g++

all:	vhost crawler scan dispatcher_daemon


dispatcher_daemon:
	$(CXX) -o dispatcher_daemon dispatcher_daemon.cc -O0 -ggdb -lpthread -lcurl

scan:
	$(CC) -o scan scan.c -lpcap  -lpthread -O2 `mysql_config --include` `mysql_config --libs`

crawler:
	$(CXX) -o crawler crawler.cc crawler_kc.cc -lkyotocabinet -lpthread -O2 -ggdb -lcurl -lcares -lbrotlienc -lbrotlicommon # `mysql_config --include` `mysql_config --libs`

crawlerkc:
	$(CXX) -o crawlerkc crawler.cc crawler_kc.cc -lpthread -O2 -ggdb -lkyotocabinet -lcurl -lcares

vhost:
	$(CXX) -o captcha.o -c captcha.cc  -O0 -ggdb
	$(CC) -o vhost.o -c vhost.c `mysql_config --include`  -O0 -ggdb
	$(CXX) -o vhost vhost.o captcha.o  -ltesseract -llept -lpthread -lcurl -ggdb `mysql_config --include` `mysql_config --libs ` -lpcre

clean:
	rm -rf scan crawler vhost dispatcher_daemon *.o

