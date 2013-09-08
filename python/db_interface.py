
import MySQLdb

dbuser = "wwitt"
dbpass = "112233w"
dbhost = "127.0.0.1"
dbname = "wwitt"

class DBInterface():
	def __init__(self):
		self.db = MySQLdb.connect(dbhost,dbuser,dbpass,dbname)
		
	def insert(self,table,dic):
		cursor = self.db.cursor()
		values = [ '"'+x+'"' if type(x) is not int else str(x) for x in dic.values() ]
		print "INSERT INTO "+table+" (" + (",".join(dic.keys())) + ") VALUES (" + (",".join(values)) + ")"
		cursor.execute("INSERT INTO "+table+" (" + (",".join(dic.keys())) + ") VALUES (" + (",".join(values)) + ")")
		cursor.close()
		self.db.commit()
	
	def select(self,table,cond):
		cursor = self.db.cursor()
		conds =  [ x + "=" + ('"'+x+'"' if type(x) is not int else str(x)) for x in cond.keys() ]
		cursor.execute("SELECT * FROM "+table+" WHERE " + (" AND ".join(conds)) + ") VALUES (" + (",".join(values)) + ")")
		cursor.close()
		self.db.commit()

