import sys
import brotli
from kyotocabinet import *
import http_pb2

db = DB()
db.open(sys.argv[1], DB.OREADER)

cur = db.cursor()
cur.jump()
ret = cur.get(True)

while ret:
    web = http_pb2.HttpWeb()
    proto = brotli.decompress(ret[1])
    web.ParseFromString(proto)
    print ret[0]
    print web
    ret = cur.get(True)

