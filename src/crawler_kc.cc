
#include <iostream>
#include <vector>
#include <string>
#include <unistd.h>
#include <kcpolydb.h>
#include <stdlib.h>
#include <stdio.h>
#include "crawler_kc.h"
#include "crawler.h"

DBKC::DBKC(bool bannercrawl, std::string args) 
  : DBIface(bannercrawl) {

	if (!db.open(args, kyotocabinet::TreeDB::OWRITER | kyotocabinet::TreeDB::ONOREPAIR)) {
	    std::cerr << "DB open error: " << db.error().name() << std::endl;
		exit(1);
	}

	cur = db.cursor();
	cur->jump();
}

DBKC::~DBKC() {
	db.close();
}


bool DBKC::next(std::vector <std::string> & resultset) {
	resultset.clear();
	std::string key, value;

	do {
		if (!cur->get(&key, &value, true))
			return false;
	} while (value.size() > 0);

	resultset.push_back(key);
	return true;
}

void DBKC::updateService(uint32_t ip, unsigned short port, std::string data) {
	// NOP for now, not supported
}

void DBKC::updateVhost(const std::string &vhost, const std::string &url,
                      std::string data, int flag) {
	
	if (data.empty()) {
		char t = 0;
		db.set(vhost.c_str(), vhost.size(), &t, 1);
	}else{
		db.set(vhost.c_str(), vhost.size(), data.c_str(), data.size());
	}
}


