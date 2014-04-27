import plyvel
import sys
import gzip

db = plyvel.DB(sys.argv[1], create_if_missing=True)
f = gzip.open(sys.argv[2], "r")

c = 0
wb = db.write_batch()
for key in f:
        wb.put(key[:-1], b'')
        c += 1
        if (c > 1000):
                wb.write()
                wb = db.write_batch()

wb.write()
db.close()


