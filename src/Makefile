

all:
	gcc -o scan scan.c -lpcap  -lpthread -O2 `mysql_config --include` `mysql_config --libs`
	gcc -o crawler crawler.c -lpthread -O2 `mysql_config --include` `mysql_config --libs`
	gcc -o vhost vhost.c -lpthread -lcurl -O2 `mysql_config --include` `mysql_config --libs`

clean:
	rm -rf scan crawler vhost
