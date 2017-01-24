
#include <iostream>
#include <vector>
#include <string>
#include <unistd.h>
#include <errmsg.h>
#include <stdlib.h>
#include <stdio.h>
#include "crawler_mysql.h"
#include "crawler.h"

void DBMysql::do_query(MYSQL *c, const char *query) {
	for (unsigned i = 0; i < 2; i++) {
		if (mysql_query(c, query)) {
			if (mysql_errno(c) == CR_SERVER_GONE_ERROR) {
				printf("Reconnect due to connection drop\n");
				reconnect(&c);
			}
		}
		else
			break;
	}
}

void DBMysql::transaction(bool start) {
	if (start)
		do_query(mysql_conn_update, "START TRANSACTION");
	else
		do_query(mysql_conn_update, "COMMIT");
}

DBMysql::DBMysql(bool bannercrawl, std::string args) 
  : DBIface(bannercrawl) {

	/* Connect to database */
	printf("Connecting to mysqldb...\n");
	reconnect(&mysql_conn_select);
	reconnect(&mysql_conn_update);
	printf("Connected!\n");

	tcount = 0;

	const char * query = "SELECT `host` FROM `virtualhosts` WHERE (`head` IS null OR `index` IS null)";
	if (bannercrawl) query = "SELECT `ip`, `port` FROM `services` WHERE `head` IS null";
	do_query(mysql_conn_select, query);
	query_result = mysql_store_result(mysql_conn_select);

	transaction(true);
}

std::string mysql_real_escape_std_string(MYSQL * c, const std::string s) {
	char tempb[s.size()*2+2];
	mysql_real_escape_string(c, tempb, s.c_str(), s.size());
	return std::string(tempb);
}

bool DBMysql::next(std::vector <std::string> & resultset) {
	MYSQL_ROW row;
	row = mysql_fetch_row(query_result);
	if (!row) return false;
	
	resultset.clear();
	resultset.push_back(std::string(row[0]));
	if (bannercrawl)
		resultset.push_back(std::string(row[1]));
	return true;
}

void DBMysql::reconnect(MYSQL ** c) {
	if (*c)
		mysql_close(*c);

	char *server = getenv("MYSQL_HOST");
	char *user = getenv("MYSQL_USER");
	char *password = getenv("MYSQL_PASS");
	char *database = getenv("MYSQL_DB");
	
	*c = mysql_init(NULL);
	// Eanble auto-reconnect, as some versions do not enable it by default
	my_bool reconnect = 1;
	mysql_options(*c, MYSQL_OPT_RECONNECT, &reconnect);
	if (!mysql_real_connect(*c, server, user, password, database, 0, NULL, 0)) {
		fprintf(stderr, "User %s Pass %s Host %s Database %s\n", user, password, server, database);
		exit(1);
	}
}

void DBMysql::updateService(uint32_t ip, unsigned short port, std::string data) {
	char sql_query[BUFSIZE*3];
	char tempb[data.size()*2+2];
	mysql_real_escape_string(mysql_conn_update, tempb, data.c_str(), data.size());
	sprintf(sql_query,"UPDATE `services` SET `head`='%s' WHERE `ip`=%lu AND `port`=%d\n",
		tempb, ip, port);
	
	if (tcount == QUERIES_PER_TR-1) transaction(false);
	tcount = (tcount + 1) % QUERIES_PER_TR;
	if (tcount == 0) transaction(true);

	do_query(mysql_conn_update, sql_query);
}

void DBMysql::updateVhost(const std::string &vhost, const std::string &url,
	                      std::string head, std::string body, int eflag) {
	
	char sql_query[BUFSIZE*3];

	std::string head_    = mysql_real_escape_std_string(mysql_conn_update, head);
	std::string body_    = mysql_real_escape_std_string(mysql_conn_update, body);
	std::string hostname = mysql_real_escape_std_string(mysql_conn_update, vhost);
	std::string url_     = mysql_real_escape_std_string(mysql_conn_update, url);

	sprintf(sql_query, "UPDATE virtualhosts SET `index`='%s',`head`='%s',`url`='%s',`status`=`status`|%d WHERE  `host`=\"%s\";",
	        head_.c_str(), body_, url_.c_str(), eflag, hostname.c_str());
	
	if (tcount == QUERIES_PER_TR-1) transaction(false);
	tcount = (tcount + 1) % QUERIES_PER_TR;
	if (tcount == 0) transaction(true);

	do_query(mysql_conn_update, sql_query);
}

DBMysql::~DBMysql() {
	transaction(false);

	mysql_close(mysql_conn_select);
	mysql_close(mysql_conn_update);
}


