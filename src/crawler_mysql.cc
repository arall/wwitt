
#include <iostream>
#include <vector>
#include <string>
#include <unistd.h>
#include <mysql.h>
#include <errmsg.h>
#include <stdlib.h>
#include <stdio.h>
#include "crawler.h"

MYSQL *mysql_conn_select = 0;
MYSQL *mysql_conn_update = 0;

void mysql_initialize();

std::string mysql_real_escape_std_string(MYSQL * c, const std::string s) {
	char tempb[s.size()*2+2];
	mysql_real_escape_string(c, tempb, s.c_str(), s.size());
	return std::string(tempb);
}

void db_initialize() {
	mysql_initialize();
};	

MYSQL_RES *query_result = 0;
void db_query(bool bannercrawl) {
	const char * query = "SELECT `host` FROM `virtualhosts` WHERE (`head` IS null OR `index` IS null)";
	if (bannercrawl) query = "SELECT `ip`, `port` FROM `services` WHERE `head` IS null";
	mysql_query(mysql_conn_select, query);
	query_result = mysql_store_result(mysql_conn_select);
}

bool db_next(std::vector <std::string> & resultset, bool bannercrawl) {
	MYSQL_ROW row;
	row = mysql_fetch_row(query_result);
	if (!row) return false;
	
	resultset.clear();
	resultset.push_back(std::string(row[0]));
	if (bannercrawl)
		resultset.push_back(std::string(row[1]));
	return true;
}


void db_reconnect(MYSQL ** c) {
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

void mysql_initialize() {
	/* Connect to database */
	printf("Connecting to mysqldb...\n");
	db_reconnect(&mysql_conn_select);
	db_reconnect(&mysql_conn_update);
	printf("Connected!\n");
}

void db_update_service(unsigned long ip, unsigned short port, const char * data, int data_len) {
	char sql_query[BUFSIZE*3];
	char tempb[data_len*2+2];
	mysql_real_escape_string(mysql_conn_update, tempb, data, data_len);
	sprintf(sql_query,"UPDATE `services` SET `head`='%s' WHERE `ip`=%lu AND `port`=%d\n",
		tempb, ip, port);
	
	if (mysql_query(mysql_conn_update,sql_query)) {
		if (mysql_errno(mysql_conn_update) == CR_SERVER_GONE_ERROR) {
			printf("Reconnect due to connection drop\n");
			db_reconnect(&mysql_conn_update);
		}
	}
}

void db_update_vhost(const std::string & vhost, const std::string & url, 
	const char * head_ptr, int head_len, const char * body_ptr, int body_len, int eflag) {
	
	char sql_query[BUFSIZE*3];
	char tempb [body_len*2+2];
	char tempb_[head_len*2+2];

	mysql_real_escape_string(mysql_conn_update, tempb,  body_ptr, body_len);
	mysql_real_escape_string(mysql_conn_update, tempb_, head_ptr, head_len);

	std::string hostname = mysql_real_escape_std_string(mysql_conn_update,vhost);
	std::string url_     = mysql_real_escape_std_string(mysql_conn_update,url);

	sprintf(sql_query, "UPDATE virtualhosts SET `index`='%s',`head`='%s',`url`='%s',`status`=`status`|%d WHERE  `host`=\"%s\";", tempb_, tempb, url_.c_str(), eflag, hostname.c_str());
	
	if (mysql_query(mysql_conn_update,sql_query)) {
		if (mysql_errno(mysql_conn_update) == CR_SERVER_GONE_ERROR) {
			printf("Reconnect due to connection drop\n");
			db_reconnect(&mysql_conn_update);
		}
	}
}

void db_transaction(bool start) {
	if (start)
		mysql_query(mysql_conn_update, "START TRANSACTION");
	else
		mysql_query(mysql_conn_update, "COMMIT");
}

void db_finish() {
	mysql_close(mysql_conn_select);
	mysql_close(mysql_conn_update);
}

