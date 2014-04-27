
#include <iostream>
#include <vector>
#include <string>
#include <unistd.h>
#include <kcpolydb.h>
#include <stdlib.h>
#include <stdio.h>
#include "crawler.h"

kyotocabinet::PolyDB db;

void db_initialize() {
	char *file = getenv("KC_DB");
	if (!db.open(file, kyotocabinet::PolyDB::OWRITER | kyotocabinet::PolyDB::OCREATE)) {
	    std::cerr << "DB open error: " << db.error().name() << std::endl;
		exit(1);
	}
}

kyotocabinet::DB::Cursor* cur = NULL;
void db_query(bool bannercrawl) {
	cur = db.cursor();
	cur->jump();
}

bool db_next(std::vector <std::string> & resultset, bool bannercrawl) {
	resultset.clear();
	std::string key, value;

	do {
		if (!cur->get(&key, &value, true))
			return false;
	} while (value.size() > 0);

	resultset.push_back(key);
	return true;
}

void db_update_service(unsigned long ip, unsigned short port, const char * data, int data_len) {
}

void db_update_vhost(const std::string & vhost, const std::string & url, 
	const char * head_ptr, int head_len, const char * body_ptr, int body_len, int eflag) {
	
	if (head_len+body_len == 0) {
		char t = 0;
		db.set(vhost.c_str(), vhost.size(), &t, 1);
	}else{
		char * tempb = malloc(body_len + head_len + 4);
		memcpy(tempb,              head_ptr,   head_len);
		memcpy(&tempb[head_len],   "\r\n\r\n", 4       );
		memcpy(&tempb[head_len+4], body_ptr,   body_len);
		
		db.set(vhost.c_str(), vhost.size(), tempb, body_len + head_len + 4);
		free(tempb);
	}
}

void db_transaction(bool start) {
}

void db_finish() {
	db.close();
}

