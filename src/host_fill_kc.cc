
#include <stdlib.h>
#include <iostream>
#include <kcpolydb.h>

#include "http.pb.h"
#include "util.h"

int main(int argc, char ** argv) {
	kyotocabinet::TreeDB db;

	if (!db.open(argv[1], kyotocabinet::TreeDB::OWRITER | kyotocabinet::TreeDB::OCREATE)) {
		fprintf(stderr, "Error opening DB\n");
		exit(1);
	}

	// Empty entry
	httpcrawler::HttpWeb p;
	p.set_status(httpcrawler::HttpWeb::UNCRAWLED);
	std::string serialized;
	if (!p.SerializeToString(&serialized)) {
		std::cerr << "Error serializing the proto!" << std::endl;
		exit(1);
	}
	std::cerr << "Proto size is " << serialized.size() << std::endl;
	serialized = brotliarchive(serialized);
	std::cerr << "Compressed proto size is " << serialized.size() << std::endl;

	std::string dom;
	while (std::cin >> dom) {
		db.set(dom.c_str(), dom.size(), serialized.c_str(), serialized.size());
	}

	db.close();
}

