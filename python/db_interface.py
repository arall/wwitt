
import MySQLdb
from threading import Semaphore

dbuser = "wwitt"
dbpass = "112233w"
dbhost = "127.0.0.1"
dbname = "wwitt"

class DBInterface():
	def __init__(self):
		self.db = MySQLdb.connect(dbhost,dbuser,dbpass,dbname)
		self._lock = Semaphore(1)
		
	def insert(self,table,dic):
		self._lock.acquire()
		cursor = self.db.cursor()
		values = [ '"'+x+'"' if type(x) is str else str(x) for x in dic.values() ]
		cursor.execute("INSERT INTO "+table+" (" + (",".join(dic.keys())) + ") VALUES (" + (",".join(values)) + ")")
		ret = cursor.lastrowid
		cursor.close()
		self.db.commit()
		self._lock.release()
		return ret
	
	def select(self,table,cond):
		self._lock.acquire()
		cursor = self.db.cursor()
		conds =  [ x + "=" + ('"'+x+'"' if type(x) is str else str(x)) for x in cond.keys() ]
		cursor.execute("SELECT * FROM "+table+" WHERE " + (" AND ".join(conds)) + ") VALUES (" + (",".join(values)) + ")")
		cursor.close()
		self.db.commit()
		self._lock.release()

