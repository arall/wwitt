from kyotocabinet import *
import sys
import gzip

db = DB()
db.open(sys.argv[1] + ".kct", DB.OWRITER | DB.OCREATE)
f = gzip.open(sys.argv[2], "r")

c = 0
for key in f:
        db.set(key[:-1], b'')
        c += 1

db.close()

