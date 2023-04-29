#ifndef __DB__
#define __DB__

#include <string>
#ifdef USE_MYSQL
#include <mysql_connection.h>
#include <driver.h>
#include <exception.h>
#include <resultset.h>
#include <statement.h>
#include <prepared_statement.h>

#define CREATE_DB "CREATE DATABASE IF NOT EXISTS "

#define CREATE_TB_BBL1 "CREATE TABLE IF NOT EXISTS BBL_"
#define CREATE_TB_BBL2 " (\
Position INT UNSIGNED NOT NULL AUTO_INCREMENT, \
Address BIGINT(64) UNSIGNED NOT NULL,\
Img SMALLINT NOT NULL,\
PRIMARY KEY (Position)\
)"

#define CREATE_TB_EDG1 "CREATE TABLE IF NOT EXISTS EDG_"
#define CREATE_TB_EDG2 " (\
Position INT UNSIGNED NOT NULL, \
Src BIGINT(64) UNSIGNED NOT NULL,\
Dst BIGINT(64) UNSIGNED NOT NULL,\
ImgSrc SMALLINT NOT NULL,\
ImgDst SMALLINT NOT NULL,\
PRIMARY KEY (Position)\
)"

#define DELETE_TB "DROP TABLE IF EXISTS "

#define INSERT_FILE1 "LOAD DATA INFILE '"
#define INSERT_FILE2 "' INTO TABLE "
#define INSERT_FILE3 " FIELDS TERMINATED BY ' ' \
ENCLOSED BY '\"' \
LINES TERMINATED BY '\\n'"

#define CREATE_IMG1 "CREATE TABLE IF NOT EXISTS IMAGES \
( \
ID SMALLINT NOT NULL, \
Path VARCHAR(100) NOT NULL, \
SHA CHAR("
#define CREATE_IMG2 ") NOT NULL, \
Start BIGINT(64) UNSIGNED NOT NULL, \
End BIGINT(64) UNSIGNED NOT NULL, \
PRIMARY KEY (ID) \
)"


#define GET_IMGS "SELECT * FROM IMAGES"

#define INSERT_IMGS "INSERT INTO IMAGES \
(ID, Path, SHA, Start, End) VALUES \
(?, ?, ?, ?, ?)"

#define ENABLE_TRUNC "SET @@global.sql_mode= 'NO_AUTO_CREATE_USER,NO_ENGINE_SUBSTITUTION'"

#define HOST "localhost"
#define USER "trace"
#define PASS "Hj6gWNn^k4%K_WfuvYLrPe!J=SufnxN9"

#define P_PATH "/var/lib/mysql-files/"
#define SHA_NONE ""


#else
#define P_PATH "~/disk/edges/"
#define SHA_NONE "0000000000000000000000000000000000000000"
#endif
// Other than the main thread
#define THREADS 7

using namespace std;

#ifdef USE_MYSQL //MySQL database

namespace db{

	void print_sql_error(sql::SQLException e);
	

	// A wrapper over mysql database
	class database
	{
	public:
		database();
		~database();
		bool connect(string url, string user, string pass);
		bool create(string name);
		bool store_trace(string path, string tbl_name, int count);
		bool get_imgs(string program);
		bool update_imgs(string program);

	private:
		sql::Driver* driver_ = NULL;
		sql::Connection* con_ = NULL;
		string db_;
		// void blocks_table(sql::PreparedStatement *pstmt, bbl_t b, int count);
		// void edges_table(sql::PreparedStatement *pstmt, edg_t e, int count);
	};

}
#endif //MySQL database 

#endif