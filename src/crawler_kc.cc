
#include <iostream>
#include <vector>
#include <string>
#include <unistd.h>
#include <kcpolydb.h>
#include <stdlib.h>
#include <stdio.h>
#include "crawler_kc.h"
#include "crawler.h"
#include "util.h"
#include "http.pb.h"

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

std::string ipPortStr(uint32_t ip, uint16_t port) {
	std::string key = std::to_string((ip >> 24)       ) + "." +
	                  std::to_string((ip >> 16) & 0xFF) + "." +
	                  std::to_string((ip >>  8) & 0xFF) + "." +
	                  std::to_string((ip      ) & 0xFF) + ":" + std::to_string(port);
	return key;
}

void DBKC::addService(uint32_t ip, uint16_t port) {
	std::string key = ipPortStr(ip, port);
	db.set(key.c_str(), "");
}

bool DBKC::next(std::string & resultset) {
	resultset.clear();
	std::string key, value;
	httpcrawler::HttpWeb p;

	do {
		if (!cur->get(&key, &value, true))
			return false;

		std::string ser = brotlidearchive(value);
		p.ParseFromString(ser);

	} while (p.status() != httpcrawler::HttpWeb::UNCRAWLED);

	resultset = key;
	return true;
}

void DBKC::updateService(uint32_t ip, unsigned short port, std::string data) {
	// Index hosts by IP, the value contains a JSON dict indexed by port
	std::string key = ipPortStr(ip, port);
	db.set(key.c_str(), data.c_str());
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


