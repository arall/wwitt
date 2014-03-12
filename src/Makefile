

all:	vhost crawler scan dispatcher_daemon


dispatcher_daemon:
	g++ -o dispatcher_daemon dispatcher_daemon.cc -O0 -ggdb -lpthread -lcurl

scan:
	gcc -o scan scan.c -lpcap  -lpthread -O2 `mysql_config --include` `mysql_config --libs`

crawler:
	g++ -o crawler crawler.cc -lpthread -O0 -ggdb `mysql_config --include` `mysql_config --libs`

vhost:
	g++ -o captcha.o -c captcha.cc  -O0 -ggdb
	gcc -o vhost.o -c vhost.c `mysql_config --include`  -O0 -ggdb
	g++ -o vhost vhost.o captcha.o  -ltesseract -llept -lpthread -lcurl -ggdb `mysql_config --include` `mysql_config --libs ` -lpcre

clean:
	rm -rf scan crawler vhost dispatcher_daemon *.o

