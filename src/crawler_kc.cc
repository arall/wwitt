
#include <iostream>
#include <vector>
#include <string>
#include <unistd.h>
#include <kcpolydb.h>
#include <stdlib.h>
#include <stdio.h>
#include <brotli/decode.h>
#include <brotli/encode.h>
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
                      std::string head, std::string body, int flag) {
	
	if (body.empty() && head.empty()) {
		char t = 0;
		db.set(vhost.c_str(), vhost.size(), &t, 1);
	}else{
		std::string r = (head + "\r\n\r\n" + body);
		BrotliEncoderState* s = BrotliEncoderCreateInstance(0, 0, 0);
		BrotliEncoderSetParameter(s, BROTLI_PARAM_QUALITY, 10);
		BrotliEncoderSetParameter(s, BROTLI_PARAM_LGWIN, BROTLI_MAX_WINDOW_BITS);

		uint8_t *tptr = (uint8_t*)malloc(r.size()*2);
		size_t available_in = r.size();
		size_t available_out = r.size() * 2;
		const uint8_t *next_in = (uint8_t*)r.data();
		uint8_t *next_out = tptr;
		BrotliEncoderCompressStream(s, BROTLI_OPERATION_FINISH, &available_in, &next_in, &available_out, &next_out, NULL);
		BrotliEncoderDestroyInstance(s);

		db.set(vhost.c_str(), vhost.size(), (char*)tptr, available_out);

		free(tptr);
	}
}


