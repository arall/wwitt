
#include <mysql.h>
#include "crawler_iface.h"

#define QUERIES_PER_TR   100

class DBMysql : public DBIface {
public:
	DBMysql(bool bannercrawl, std::string args);
	virtual ~DBMysql();

	bool next(std::vector <std::string> & resultset) override;

	void updateService(uint32_t ip, unsigned short port, std::string data) override;
	void updateVhost(const std::string &vhost, const std::string &url,
	                 std::string head, std::string body, int flag) override;

protected:
	void reconnect(MYSQL ** c);
	void do_query(MYSQL *c, const char *query);
	void transaction(bool start);

	MYSQL *mysql_conn_select;
	MYSQL *mysql_conn_update;

	MYSQL_RES *query_result;
	unsigned tcount;
};


