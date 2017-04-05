
#include <kcpolydb.h>
#include "crawler_iface.h"


class DBKC : public DBIface {
public:
	DBKC(bool bannercrawl, std::string args);
	virtual ~DBKC();

	void addService(uint32_t, uint16_t) override;
	bool next(std::string & resultset) override;

	void updateService(uint32_t ip, unsigned short port, std::string data) override;
	void updateVhost(const std::string &vhost, const std::string &url,
	                 std::string data, int flag) override;


protected:
	kyotocabinet::TreeDB db;
	kyotocabinet::DB::Cursor* cur;
};


