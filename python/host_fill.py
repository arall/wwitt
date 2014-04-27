
import plyvel
import sys

db = plyvel.DB(sys.argv[1], create_if_missing=True)

while (True):
	try:
		key = input()
	except:
		break
	db.put(key.encode("utf-8"), b'')

db.close()


