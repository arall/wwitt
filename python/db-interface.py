
import MySQLdb

dbuser = "wwitt"
dbpass = "112233w"
dbhost = "localhost"
dbname = "wwitt"

class DBInterface():
	def __init__(self):
		self.db = MySQLdb.connect(dbhost,dbuser,dbpass,dbname)
		
	def insert(self,table,dic):
		cursor = db.cursor()
		values = [ '"'+x+'"' if type(x) is not int else str(x) for x in dic.values() ]
		cursor.execute("INSERT INTO "+table+" (" + (",".join(dic.keys())) + ") VALUES (" + (",".join(values)) + ")")
		cursor.close()
	
	def select(self,table,cond):
		cursor = db.cursor()
		conds =  [ x + "=" + ('"'+x+'"' if type(x) is not int else str(x)) for x in cond.keys() ]
		cursor.execute("SELECT * FROM "+table+" WHERE " + (" AND ".join(conds) + ") VALUES (" + (",".join(values)) + ")"
		cursor.close()

