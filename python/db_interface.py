
import mysql.connector
from threading import Semaphore

dbuser = "wwitt"
dbpass = "112233w"
dbhost = "127.0.0.1"
dbname = "wwitt"

class DBInterface():
	def __init__(self):
		self.db = mysql.connector.connect(host=dbhost,user=dbuser,password=dbpass,database=dbname)
		self._lock = Semaphore(1)
		
	def insert(self,table,dic):
		values = [ str(x) for x in dic.values() ]
		query = "INSERT INTO "+table+" (" + (",".join(dic.keys())) + ") VALUES (" + (",".join(['%s']*len(values))) + ")"

		self._lock.acquire()
		try:
			ret = None
			cursor = self.db.cursor()
			cursor.execute(query,values)
			ret = cursor.lastrowid
		except Exception as err:
			pass

		cursor.close()
		self.db.commit()

		self._lock.release()
		return ret
	
	def select(self,table,fields = "*",cond = {}):
		c_eq = { x:cond[x]            for x in cond if "!" not in x and "$" not in x and cond[x] != "$NULL$" }
		c_neq= { x.strip("!"):cond[x] for x in cond if "!" in x and cond[x] != "$NULL$" }   # Not equal conditions
		c_isn= { x:cond[x]            for x in cond if "!" not in x and "$" not in x and cond[x] == "$NULL$" }
		c_non= { x.strip("!"):cond[x] for x in cond if "!" in x and cond[x] == "$NULL$" }   # Not equal conditions
		
		c1 = [ (str(x) + "= %s", str(c_eq[x])) for x in c_eq.keys()  ]
		c2 = [ (str(x) + "<> %s", str(c_neq[x])) for x in c_neq.keys() ]
		c3 = [ (str(x) + " is null","")     for x in c_isn.keys() ]
		c4 = [ (str(x) + " is not null","") for x in c_non.keys() ]
		allc = c1+c2+c3+c4
		queryc = [ x[0] for x in (c1+c2+c3+c4) ]
		queryv = [ x[1] for x in (c1+c2) ]

		query = "SELECT "+fields+" FROM "+table
		if (len(allc) != 0): query += " WHERE " + (" AND ".join(queryc))

		self._lock.acquire()
		try:
			cursor = self.db.cursor()
			cursor.execute(query,queryv)
			ret = cursor.fetchall()
			cursor.close()
			self.db.commit()
		except Exception as e:
			print ("Error!",e)
			pass

		self._lock.release()

		# If only one filed, convert lists of lists to list
		if (len(fields.split(",")) == 1):
			return ( x[0] for x in ret )
		return ret

	def update(self,table,cond = {},values = {}):
		conds =  [ table+"."+str(x) + "= %s " for x in cond.keys() ]
		vals  =  [ table+"."+str(x) + "= %s " for x in values.keys() ]
		query = "UPDATE "+table+" "
		query += " SET " + (" , ".join(vals))
		if (len(conds) != 0): query += " WHERE " + (" AND ".join(conds))

		self._lock.acquire()
		try:
			cursor = self.db.cursor()
			cursor.execute( query, list(values.values()) + list(cond.values()) )
		except Exception as e:
			print ("DB ERROR",e)
			pass

		cursor.close()
		self.db.commit()
		self._lock.release()



