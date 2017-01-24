
#include <stdlib.h>
#include <iostream>
#include <kcpolydb.h>

int main(int argc, char ** argv) {
	kyotocabinet::TreeDB db;

	if (!db.open(argv[1], kyotocabinet::TreeDB::OWRITER | kyotocabinet::TreeDB::OCREATE)) {
		fprintf(stderr, "Error opening DB\n");
		exit(1);
	}

	std::string dom;
	while (std::cin >> dom) {
		db.set(dom.c_str(), ""); //vhost.size(), &t, 1);
	}

	db.close();
}

