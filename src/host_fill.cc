
#include <stdlib.h>
#include <iostream>
#include <leveldb/db.h>

int main(int argc, char ** argv) {
	leveldb::DB* db;
	leveldb::Options options;
	options.create_if_missing = true;
	
	leveldb::Status status = leveldb::DB::Open(options, argv[1], &db);
	if (!status.ok()) {
		fprintf(stderr, "Error opening DB\n");
		exit(1);
	}
	
	std::string domain;
	while (std::cin >> domain) {
		db->Put(leveldb::WriteOptions(), domain, "");
	}

	delete db;
}

