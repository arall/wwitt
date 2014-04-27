
void db_initialize();
void db_query(bool bannercrawl);
bool db_next(std::vector <std::string> & resultset, bool bannercrawl);

void db_update_service(unsigned long ip, unsigned short port, const char * data, int data_len);
void db_update_vhost(const std::string & vhost, const std::string & url,
	const char * head_ptr, int head_len, const char * body_ptr, int body_len, int flag);
void db_transaction(bool start);
void db_finish();

