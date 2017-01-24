
#include <kcpolydb.h>
#include "crawler_iface.h"


class DBKC : public DBIface {
public:
	DBKC(bool bannercrawl, std::string args);
	virtual ~DBKC();

	bool next(std::vector <std::string> & resultset) override;

	void updateService(uint32_t ip, unsigned short port, std::string data) override;
	void updateVhost(const std::string &vhost, const std::string &url,
	                 std::string head, std::string body, int flag) override;


protected:
	kyotocabinet::TreeDB db;
	kyotocabinet::DB::Cursor* cur;
};


