import mysql.connector
import sys
from collections import defaultdict




Q_GetTable = "SELECT TABLE_NAME FROM information_schema.TABLES WHERE TABLE_SCHEMA = '{db}'"


# Q_MakeDB = "CREATE DATABASE IF NOT EXISTS EDG_{db}"

# Q_MakeEdges = \
# "CREATE VIEW pairs_view \
# AS ( \
# SELECT x.Position Pos, x.Address Src, y.Address Dst, x.Img ImgSrc, y.Img ImgDst \
# FROM {0} x \
# LEFT JOIN {0} y \
# ON x.Position + 1 = y.Position \
# ORDER BY x.Position \
# )"

# Q_GetDsitinctEdges = \
# "CREATE TABLE IF NOT EXISTS {0}.{1} \
# AS ( \
# SELECT min(Pos) AS Pos, Src, Dst, ImgSrc, ImgDst FROM pairs_view \
# GROUP BY Src, Dst, ImgSrc, ImgDst \
# ORDER BY min(Pos)\
# )"

# Q_DropPairsView = "DROP VIEW IF EXISTS pairs_view"


Q_FindCommon = "CREATE TABLE {out} \
AS ( \
SELECT LEAST(T0.TID, T1.TID) TID, LEAST(T0.Position, T1.Position) Position, T0.Src, T0.Dst, T0.ImgSrc, T0.ImgDst \
FROM {t1} T0 JOIN {t2} T1 \
ON T0.Src = T1.Src AND T0.Dst = T1.Dst AND T0.ImgSrc = T1.ImgSrc AND T0.ImgDst = T1.ImgDst \
ORDER BY TID, Position \
)"

Q_FindUniq = "CREATE TABLE {out} \
SELECT T0.TID, T0.Position, T0.Src, T0.Dst, T0.ImgSrc, T0.ImgDst \
FROM {t1} T0  LEFT JOIN {t2} T2 \
ON T0.Src = T2.Src AND T0.Dst = T2.Dst AND T0.ImgSrc = T2.ImgSrc AND T0.ImgDst = T2.ImgDst \
WHERE T2.Src IS NULL"

Q_UnionThreads = "CREATE TABLE {out} \
SELECT min(TID) AS TID, Position, Src, Dst, ImgSrc, ImgDst \
FROM (\
SELECT {thread1} AS TID, Position, Src, Dst, ImgSrc, ImgDst \
FROM {t1} \
UNION \
SELECT {thread2} AS TID,  Position, Src, Dst, ImgSrc, ImgDst \
FROM {t2} \
) t \
GROUP BY Src, Dst, ImgSrc, ImgDst"

Q_CopyTable = "CREATE TABLE {dst} AS SELECT * FROM {src}"

Q_CopyThread = "CREATE TABLE {dst} AS SELECT {thread} AS TID,  Position, Src, Dst, ImgSrc, ImgDst FROM {src}"

Q_DropTable = "DROP TABLE IF EXISTS {tbl}"

Q_RenameTable = "RENAME TABLE {src} to {dst}"

Q_ShowDBs = "SHOW DATABASES"

DB_NAME = None

DB = mysql.connector.connect(
	 host = "localhost",
	 user = "trace",
	 # password = "gKGkQD3F5BHaCcJhZ5EAu2xGW8rhdpHw")
	 password = 'Hj6gWNn^k4%K_WfuvYLrPe!J=SufnxN9')

c = DB.cursor()


def init():
	global DB
	global c
	DB = mysql.connector.connect(
		 host = "localhost",
		 user = "trace",
		 database  = DB_NAME,
		 # password = "gKGkQD3F5BHaCcJhZ5EAu2xGW8rhdpHw")
		 password = 'Hj6gWNn^k4%K_WfuvYLrPe!J=SufnxN9')

	c = DB.cursor()

stdDBs = ['information_schema', 'performance_schema', 'sys', 'mysql']

# def distinct_edges(tables):
# 	c.execute(Q_DropPairsView)
# 	for t in tables:
# 		if(t.startswith('BBL')):
# 			print "Processing {}".format(t)
# 			c.execute(Q_MakeEdges.format(t))
# 			c.execute(Q_GetDsitinctEdges.format("EDG_" + DB_NAME,t.replace('BBL','EDG')))
# 			c.execute(Q_DropPairsView)
# 	DB.commit();


def merge_threads(threads):
	for f, thr_list in threads.iteritems():
		print "merging threads in table " + f 
		c.execute(Q_DropTable.format(tbl=f))
		if(len(thr_list) > 1 ):
			thread1 = thr_list[0]
			thread2 = thr_list[1]
			tid1 = int(thread1[-1])
			tid2 = int(thread2[-1])
			c.execute(Q_UnionThreads.format(out=f, thread1=tid1 , t1=f+'_'+thread1, thread2=tid2 , t2=f+'_'+thread2))
			if(len(thr_list) > 2):
				for t in thr_list[2:]:
					thread = int(t[-1]) 
					c.execute(Q_UnionThreads.format(out="TMP", thread1='TID' , t1=f, thread2=thread , t2=f+'_'+t))
					c.execute(Q_DropTable.format(tbl=f))
					c.execute(Q_RenameTable.format(src="TMP",dst=f))
		else:
			t = f + '_' +thr_list[0]
			c.execute(Q_CopyThread.format(src=t, dst=f, thread=int(thr_list[0][-1])))

def common_accross_instances(instances, target):
	f = target
	t_list = instances[target]
	print "Creating CMN_" + f
	c.execute(Q_DropTable.format(tbl="CMN_" + f))
	if(len(t_list) > 1):
		t1 = t_list[0]
		t2 = t_list[1]
		c.execute(Q_FindCommon.format(out="CMN_" + f, t1=t1, t2=t2))
		if(len(t_list) > 2):
			for t in t_list[2:]:
				c.execute(Q_FindCommon.format(out="TMP",t1="CMN_" + f, t2=t))
				c.execute(Q_DropTable.format(tbl="CMN_" + f))
				c.execute(Q_RenameTable.format(src="TMP",dst="CMN_" + f))
	else:
		t = t_list[0]
		c.execute(Q_CopyTable.format(src=t, dst="CMN_" + f))
	DB.commit();

def uniq_accros_features(instances, target):
	print "Creating UNQ_" + target
	c.execute(Q_DropTable.format(tbl="UNQ_" + target))
	c.execute(Q_CopyTable.format(src="CMN_" + target, dst="UNQ_" + target))
	for f, t_list in instances.iteritems():
		if f != target:
			for t in t_list:
				c.execute(Q_FindUniq.format(out="TMP", t1="UNQ_" + target, t2=t))
				c.execute(Q_DropTable.format(tbl="UNQ_" + target))
				c.execute(Q_RenameTable.format(src="TMP", dst="UNQ_" + target))
	DB.commit();


# def get_edge_tables():
# 	edg_tables = list()
# 	c.execute(Q_GetTable.format(db="EDG_" + DB_NAME))
# 	r = c.fetchall()
# 	for x in r:
# 		if str(x[0]).startswith('EDG_'):
# 			edg_tables.append(str(x[0]))
# 	return edg_tables


def get_tables():
	tables = list()
	c.execute(Q_GetTable.format(db=DB_NAME))
	r = c.fetchall()
	for x in r:
		if str(x[0]) != 'IMAGES':
			tables.append(str(x[0]))
	return tables

# def make_edg_db():
# 	c.execute(Q_MakeDB.format(db=DB_NAME))

def get_threads_dict(edg_tables):
	threads = defaultdict(list)
	for t in edg_tables:
		if 'thread' in t:
			threads['_'.join(t.split('_')[0:-1])].append(t.split('_')[-1])
	return threads

def get_inst_dict(threads):
	instances = defaultdict(list)
	for t in threads:
		instances[t.split('_')[1]].append(t)
	return instances

def show_dbs():
	databases = []
	c.execute(Q_ShowDBs)
	r = c.fetchall()
	for x in r:
		if str(x[0]) in stdDBs or str(x[0]).startswith('EDG_'):
			continue
		else:
			databases.append(str(x[0]))
	for i, d in enumerate(databases):
		print "%d\t%s" % (i,d)

	return databases

def get_db(databases):
	while True:
		n = int(raw_input("please type db number: "))
		if n < 0 or n > len(databases)-1:
			print "wrong number"
		else:
			break
	global DB_NAME
	DB_NAME = databases[n]

def print_usage():
	print "Usage: findedg.py [options]"
	print "  options:  --no-merge\tSkip merging threads"
	print "            --no-common\tSkip finding common edges in each feature"


def main(argv):


	no_merge = False
	no_common = False


	if len(argv) == 2:
		if argv[1]  == '--no-merge':
			no_merge = True
		else:
			print "unkown argument"
			print_usage()
			exit(-1)
	

	get_db(show_dbs())
	init()


	if not DB or not c:
		print "ERROR: error initializing"


	tables = get_tables()
	threads = get_threads_dict(tables)
	
	if no_merge:
		print "skiping merging threads"
	else:	
		merge_threads(threads)

	instances = get_inst_dict(threads)
	
	print "Choose your target feature:"
	for i, inst in enumerate(instances):
		print "%d\t%s" % (i,inst)
	
	while True:
		n = int(raw_input("please type feature number: "))
		if n < 0 or n > len(instances)-1:
			print "wrong number"
		else:
			break
	target = instances.keys()[n]
	common_accross_instances(instances, target)

	
	uniq_accros_features(instances, target)

if __name__ == '__main__':
	argv = sys.argv
	main(argv)