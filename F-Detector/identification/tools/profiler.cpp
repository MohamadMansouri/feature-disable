// #include <string>
// #include <boost/core/demangle.hpp>

#if USE_MYSQL == 1
#include <mysql_connection.h>
#include <driver.h>
#include <exception.h>
#include <resultset.h>
#include <statement.h>
#include <prepared_statement.h>
#endif


#include <tuple>
#include <fstream>
#include <regex>
#include <thread>
#include <mutex>
#include <chrono>
#include <openssl/sha.h>
#include <set>
#include <dirent.h>
#include <signal.h>
#include <iomanip>
#include <sys/stat.h>
#include <iostream>
#include <assert.h>

#include "profiler.hpp"
#include "common.hpp"
#include "image.hpp"	

using namespace std;


static mutex mtx;

#if USE_MYSQL == 1
// database objects
static map<string, db::database> trace_db;

#else 
static string outdir;
#endif

// A mapping from the trace file path to an <program,feature,image,processed?>  
typedef map<string,tuple<string,string,img_map_t,bool>> trace_map_t;

// contains all the traces need to be processed
static trace_map_t trace_consumer;

// A set of all program names that are processed
static set<string> programs;

// A registry containing the image id to image info of each program
static img_reg_t img_registry;

// contains the new images informations need to be added to the Image table in the db
static img_reg_t img_update;


#if USE_MYSQL == 1 //use MySQL database

void db::print_sql_error(sql::SQLException e){
	// cout << "# ERR: SQLException in " << __FILE__;
	// cout << "(" << __FUNCTION__ << ") on line "
	//     << __LINE__ << endl;
	mtx.lock();
	cerr << "# ERR: " << e.what();
	cerr << " (MySQL error code: " << e.getErrorCode();
	cerr << ", SQLState: " << e.getSQLState() << " )" << endl;
	mtx.unlock();
}


db::database::database(){
	this->driver_ = get_driver_instance();
}

db::database::~database(){
	if(this->con_)
		delete this->con_;
}

bool db::database::connect(string url, string user, string pass){
	try	{
		this->con_ = driver_->connect(url, user, pass);
		return this->con_->isValid();
	}
	catch (sql::SQLException &e){
		db::print_sql_error(e);
		return false;
	}
}

bool db::database::create(string name){
	try{
		string q_create(CREATE_DB);
		q_create += name;

		sql::Statement *stmt = this->con_->createStatement();
		// mtx.lock();
		// cout << "SQL: creating database \"" <<  name << "\"" << endl;
		// mtx.unlock();
		stmt->execute(q_create);
		this->db_ = name;
		this->con_->setSchema(this->db_);
		stmt->execute(ENABLE_TRUNC);
		delete stmt;
		return true;
	}
	catch (sql::SQLException &e){
		db::print_sql_error(e);
		return false;
	}
}



bool db::database::store_trace(string path, string tbl_name, int count){
	if(!this->con_ || !this->con_->isValid()){
		cerr << "cannot store trace because connection was closed ... skiping this trace" << endl;
		return false;
	}
	string q_create_tbl(CREATE_TB_EDG1);
	q_create_tbl += tbl_name;
	q_create_tbl += CREATE_TB_EDG2;
	

	string q_drop_tbl(DELETE_TB);
	q_drop_tbl += tbl_name;
	string q_isnert(INSERT_FILE1);
	q_isnert += path;
	q_isnert += INSERT_FILE2;
	q_isnert += this->db_;
	q_isnert += ".EDG_";
	q_isnert += tbl_name;
	q_isnert += INSERT_FILE3;
	// cout << q_isnert << endl;
	try{
		sql::Statement *stmt = this->con_->createStatement();
		stmt->execute(q_drop_tbl);
		auto start = chrono::steady_clock::now();
		stmt->execute(q_create_tbl);
		stmt->execute(q_isnert);
		delete stmt;
		auto end = chrono::steady_clock::now();
		double elapsed_secs = chrono::duration_cast<chrono::seconds>(end - start).count();

		// mtx.lock();
		// cout << "FINISH: Stored " << count << " blocks in \"" << tbl_name 
		// << "\"... Elapsed time: " << elapsed_secs << " secs ... "
		// << elapsed_secs/count << " sec/insertion" << endl;
		// mtx.unlock();
		return true;
	}
	catch (sql::SQLException &e){
		db::print_sql_error(e);
		return false;
	}

}	

bool db::database::get_imgs(string program){
	if(!this->con_ || !this->con_->isValid())
		return false;
	
	string q_create(CREATE_IMG1);
	q_create += to_string(2*SHA_DIGEST_LENGTH);
	q_create += CREATE_IMG2;
	string q_get_table(GET_IMGS);

	try{
		sql::Statement *stmt = this->con_->createStatement();
		sql::ResultSet *res;
		stmt->execute(q_create);
		res = stmt->executeQuery(q_get_table);
		img_t img_reg;
		while(res->next()){
			img_reg.seqno = res->getInt("ID");
			img_reg.name = res->getString("Path");
			img_reg.sha1 = res->getString("SHA");
			img_reg.addr = res->getInt("Start");
			img_reg.end = res->getInt("End");
			img_registry[program][img_reg.seqno] = img_reg;
		}
		delete stmt;
		delete res;
		return true;
	}
	catch (sql::SQLException &e){
		db::print_sql_error(e);
		return false;
	}
}

bool db::database::update_imgs(string program){
	if(!this->con_ || !this->con_->isValid())
		return false;

	string q_insert(INSERT_IMGS);

	try{
		sql::PreparedStatement *pstmt = this->con_->prepareStatement(q_insert);

		for(img_map_t::iterator it = img_update[program].begin();
		it != img_update[program].end(); ++it ){
			pstmt->setInt(1,it->second.seqno);
			pstmt->setString(2,it->second.name);
			pstmt->setString(3,(it->second.sha1).empty()?"":it->second.sha1);
			pstmt->setUInt64(4,it->second.addr);
			pstmt->setUInt64(5,it->second.end);
			pstmt->execute();
		}

		delete pstmt;
		return true;
	}
	catch (sql::SQLException &e){
		db::print_sql_error(e);
		return false;
	}
}

#endif //use MySQL database


int create_plain(string path, img_map_t imgs ,string p_path) {
	ifstream trace(path, ifstream::in);
	if (!trace.is_open()){
		cerr << "cannot open " << path << " ... skiping this trace" << endl;
		return -1;
	}

#if USE_MYSQL == 1	
	ofstream p_stream(p_path, ofstream::out);
#else
	ofstream p_stream(p_path, ofstream::binary);
#endif

	if(!p_stream.is_open()){
		cerr << "cannot write to " << p_path << " ... skiping this trace" << endl;
		return -1;
	}

	uint64_t count = 0;
	bbl_t b, pb;
	
	if(!trace.read((char*)(&pb), sizeof(bbl_t))){
		cerr << "trace file "<< path << " is empty" << endl;
	}
	
	map<edg_t, pair<uint64_t,int>> edges;
	map<edg_t,pair<uint64_t,int>>::iterator e_it;
    // TODO: read in batches
	while(trace.read((char*)(&b), sizeof(bbl_t))){
		count++;
        edg_t e(pb.addr, b.addr, imgs[pb.imgno].seqno, imgs[b.imgno].seqno);
        
		if(edges.find(e) == edges.end())
			edges[e] = make_pair(count,1);
		else
			edges[e].second++;

		pb = b;
	}
    // TODO: Write in batches
	for(e_it = edges.begin(); e_it != edges.end(); ++e_it){

#if USE_MYSQL == 1		
		ostringstream os;

		os << e_it->second << ' ' << e_it->first.src << ' ' << 
			e_it->first.dst << ' ' << e_it->first.imgsrc <<
			' ' << e_it->first.imgdst << endl;

		p_stream << os.str();
		os.clear();
#else

		p_stream.write((char*)&(e_it->second),sizeof(pair<uint64_t,int>));
		p_stream.write((char*)&(e_it->first),sizeof(edg_t));

		// os << setw(10) << setfill('0') << e_it->second << " " <<
		// 	setw(2) << setfill('0') << imgs[e_it->first.imgsrc].seqno << ":" <<
		//  	hex << "0x" << setw(8) << setfill('0') << e_it->first.src << dec << " " <<
		// 	setw(2) << setfill('0') << imgs[e_it->first.imgdst].seqno << ":" << 
		// 	hex << "0x" << setw(8) << setfill('0') << e_it->first.dst << dec << endl;

#endif		
	}
	return edges.size();
}


void update_image_registry(string program, img_map_t* imgs){
	
	if (img_registry.find(program) == img_registry.end()){
		img_registry[program] = *imgs;
		img_update[program] = *imgs;
		return;
	}
		
	for (img_map_t::iterator it_i = imgs->begin();
		it_i != imgs->end(); ++it_i){
		bool found = false;
		for (img_map_t::iterator it_r = img_registry[program].begin();
			it_r != img_registry[program].end(); ++it_r){

			if(it_i->second.name == it_r->second.name &&
			   it_i->second.sha1 == it_r->second.sha1 /*&&
			   it_i->second.addr == it_r->second.addr &&
			   it_i->second.end == it_r->second.end*/){	
			   it_i->second.seqno = it_r->first;
			   found = true;
			   break;
			}
		}
		if(!found){
			imgseq_t seq = img_registry[program].rbegin()->first+1;
			it_i->second.seqno = seq;
			img_registry[program][seq] = it_i->second;	
			img_update[program][seq] = it_i->second;
		}
	}			
}

bool get_file_hash(const char *fileName, unsigned char* result){

	unsigned char hash[SHA_DIGEST_LENGTH];
	int i;
	FILE *f = fopen(fileName,"rb");
	SHA_CTX mdContent;
	int bytes;
	unsigned char data[1024];

	if(f == NULL){
	    return false;
	}

	SHA1_Init(&mdContent);
	while((bytes = fread(data, 1, 1024, f)) != 0){

	    SHA1_Update(&mdContent, data, bytes);
	}

	SHA1_Final(hash,&mdContent);

	for(i=0; i < SHA_DIGEST_LENGTH;i++){
	    sprintf((char *)&(result[i*2]), "%02x",hash[i]);
	}

	fclose(f);
	return true;
}

img_map_t get_imgs(string name){

	img_map_t imgs;
	if (FILE *fp = fopen(name.c_str(), "r")) {
		char* line = NULL;
		size_t len = 0;
	    while (getline(&line, &len, fp) != -1) {
			img_t img;
			char tmp_name[100];
			sscanf(line, "I: %ld %lx %lx # %s", &img.seqno, &img.addr, &img.end, tmp_name);
			img.name = string(tmp_name);
			unsigned char hash[2*SHA_DIGEST_LENGTH];
			if(get_file_hash(img.name.c_str(), hash))
				img.sha1 = string((char*)hash);
			else
				img.sha1 = string(SHA_NONE);
			imgs[img.seqno] = img;
	    }
	    free(line);
	}
	return imgs;
}



bool process_dir(char* argv){
	
 	DIR* dir;
	dirent* pdir;
	dir = opendir(argv);
	if(!dir){
		cerr << "invalid directory " << argv << 
		" ... skipping this trace" << endl; 
		return false;
	}

	regex r1("^.*\\.image$");
	regex r2("^([^_]*)_([^_]*_[^_]*_[0-9]_thread[0-9]+)$");
	smatch m;
	
	img_map_t imgs;
	vector<string> traces;
	vector<string>::iterator it;
	while ((pdir = readdir(dir))) {
		if(!strcmp(pdir->d_name,".") || !strcmp(pdir->d_name,".."))
			continue;

		string full_path = string(argv);
		full_path += "/";
		full_path += pdir->d_name;

		if(regex_search(pdir->d_name,r1)){
			imgs = get_imgs(full_path);
			if(imgs.empty()){
				cerr << "couldnt parse image file " << 
				" ... skipping this trace" << endl; 
				return false;
			}
		}

		else{
			traces.push_back(string(pdir->d_name));
		}
	}
	closedir(dir);

	for(it = traces.begin(); it != traces.end(); ++it){

		string full_path = string(argv);
		full_path += "/";
		full_path += *it;

		if(!regex_match(*it, m, r2)){
			cerr << "trace file name invalid." <<
			" ... skiping this trace : " << *it << endl;
			continue;
		}
		trace_consumer[full_path] = make_tuple(string(m[1]), string(m[2]), imgs, false);
		programs.insert(string(m[1]));
	}
	

	return true;
}

// Store each trace file into EDG table
void task_thread(int* count){

	for (trace_map_t::iterator it = trace_consumer.begin();
		it != trace_consumer.end(); ++it) {

		mtx.lock();
		if(!get<3>(it->second)){
			get<3>(it->second) = true;
			mtx.unlock();
		}
		else{
			mtx.unlock();
			continue;
		}
		string path = it->first;
		string program = get<0>(it->second);
		string tbl_name = string(get<1>(it->second));

		mtx.lock();
		update_image_registry(program, &get<2>(it->second));
		mtx.unlock();

		stringstream p_path; 
#if USE_MYSQL == 1		
		p_path << P_PATH << this_thread::get_id();
#else
		p_path << outdir << "/threads";
		
		DIR* dir = opendir(p_path.str().c_str());
		if (dir) {
		    closedir(dir);
		} else if (ENOENT == errno) {
			mkdir(p_path.str().c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
		} 

		p_path << "/" << tbl_name;
#endif		
		int count_blk = 0;
		// mtx.lock();
		// cout << "PROCESSING: " << program << "_" << tbl_name << endl;  
		// mtx.unlock();

		if((count_blk = create_plain(path, get<2>(it->second) , p_path.str())) <= 0){
			mtx.lock();
			cerr << "error reading binary file" << 
				" ... skiping this trace" << endl;
			mtx.unlock();
			continue; 
		}
		mtx.lock();
		(*count)++;

		cout << "[-] Profiling all the traced threads: " << *count << "/" << trace_consumer.size() << "\r";   
		cout.flush();
		mtx.unlock();

#if USE_MYSQL == 1

		db::database cnx = db::database();

		if(!cnx.connect(HOST, USER, PASS)){
			cerr << endl << "failed to connect to the db" << 
				" ... skiping this trace" << endl;
			continue;
		}

		if(!cnx.create(program)){
			cerr << endl << "failed to create the db" <<
				" ... skiping this trace" << endl;
			continue;	
		}
		
		if(cnx.store_trace(p_path.str(), tbl_name, count_blk)){
			mtx.lock();
			(*count)++;
			cout << "[-] Profiling all the traced threads: " << *count << "/" << trace_consumer.size() << "\r";   
			cout.flush();

			// cout << "Finished " << *count << "/" << trace_consumer.size() << endl;   
			mtx.unlock();
		}

#endif

	}
}

bool read_image_registry(){
	for(set<string>::iterator it = programs.begin();
		it != programs.end(); ++it){


#if USE_MYSQL == 1
		
		db::database cnx = db::database();
	
		if(!cnx.connect(HOST, USER, PASS))
			return false;

		if(!cnx.create(*it))
			return false;

		if(!cnx.get_imgs(*it))
			return false;
#else
		stringstream iss;
		// iss << outdir << *it;
		iss << outdir;  // assume all traces belong to the same program

		DIR* dir = opendir(iss.str().c_str());
		if (dir) {
		    closedir(dir);
		} else if (ENOENT == errno) {
			mkdir(iss.str().c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
		}

		iss << "/IMAGES";
		
		if(FILE *fp = fopen(iss.str().c_str(), "r")){	
			char* line = NULL;
			size_t len = 0;
		    while (getline(&line, &len, fp) != -1) {
				img_t img_reg;
				char tmpname[100], tmpsha[ SHA_DIGEST_LENGTH*2 +1];
				sscanf(line, "%ld %lx %lx # %s %s", &img_reg.seqno, &img_reg.addr, &img_reg.end, tmpsha, tmpname);
				img_reg.name = string(tmpname);
				img_reg.sha1 = string(tmpsha);
				img_registry[*it][img_reg.seqno] = img_reg;
			}
		}
#endif		

	}
	return true;
}


bool update_image_table(){
	for(set<string>::iterator it = programs.begin();
		it != programs.end(); ++it){
		
		if(img_update.empty()){
			// cout << "image table already up to-date" << endl;
			return true;
		}

#if USE_MYSQL == 1

		db::database cnx = db::database();
	
		if(!cnx.connect(HOST, USER, PASS))
			return false;

		if(!cnx.create(*it))
			return false;

		if(!cnx.update_imgs(*it))
			return false;
#else
		stringstream iss;
		iss << outdir << "/IMAGES";
		
		ofstream img_file;
  		img_file.open (iss.str(), ofstream::out | ofstream::app);
		
		if(!img_file.is_open())
			return false;

		for(img_map_t::iterator it2 = img_update[*it].begin();
			it2 != img_update[*it].end(); ++it2 ){
			img_file << setw(2) << setfill('0') << it2->second.seqno << " 0x" << 
				hex << setw(16) << setfill('0') << it2->second.addr << " " <<
				hex << setw(16) << setfill('0') << it2->second.end << " # " << 
				dec <<  it2->second.sha1 << " " <<  it2->second.name << endl;
		}
#endif		

	}
	// cout << "image table succesfully updated" << endl;
	return true;
}

void dump_image_registry(){
	for (img_reg_t::iterator it1 = img_registry.begin();
		it1 != img_registry.end(); ++it1){

		cout << endl << "databse: " << it1->first  << endl;
		for (img_map_t::iterator it2 = it1->second.begin();
		it2 != it1->second.end(); ++it2){
			cout << setw(2) << setfill('0') << it2->second.seqno << " 0x" << 
				hex << setw(16) << setfill('0') << it2->second.addr << " " <<
				hex << setw(16) << setfill('0') << it2->second.end << " # " << 
				dec <<  it2->second.sha1 << " " <<  it2->second.name << endl;
		}
	}

}

void handler(int s){
	cerr << "interrupt recieved" << endl <<
	"wait untill image table updated" << endl;
	
	if(!update_image_table()){
		cerr << "failed to update the image table" << 
		" ... please update it manually" << endl;
		cerr << "IMAGE REGISTRY DUMP: " << endl;
		dump_image_registry();
		exit(-1);
	}
	exit(0);
}

int main(int argc, char* argv[]){

	if(argc < 2){
		cerr << "Usage: " << argv[0] << " <dir1 dir2 ... outdir>" << endl;
		exit(-1);
	}

	struct sigaction sigIntHandler;

	sigIntHandler.sa_handler = handler;
	sigemptyset(&sigIntHandler.sa_mask);
	sigIntHandler.sa_flags = 0;

	sigaction(SIGINT, &sigIntHandler, NULL);

	assert(argc > 1);
	outdir = argv[argc -1];
	if(outdir[outdir.size() - 1] != '/')
		outdir += "/";
	//parse the names of trace file and write plain contents
	for (int i = 1; i < argc - 1; ++i) 
		process_dir(argv[i]);


	if(!read_image_registry()){
		cerr << "failed to get the image registry" << endl;
		exit(-1);
	}

	cout << "[-] Profiling all the traced threads: 0/" << trace_consumer.size() << "\r" ;
	cout.flush();
	// Create threads to do the db storing
	int count = 0;
	vector<thread> threads;
	for (int i = 0; i < THREADS; ++i) {
		threads.push_back(thread(task_thread, &count));
	}

	task_thread(&count);

	// join all threads
	for (vector<thread>::iterator th_it = threads.begin();
			th_it != threads.end(); ++th_it){
		if(th_it->joinable())
			th_it->join(); 
	}
	cout << endl;

	if(!update_image_table()){
		cerr << "failed to update the image table" << 
		" ... please update it manually" << endl;
		cerr << "IMAGE REGISTRY DUMP: " << endl;
		dump_image_registry();
		exit(-1);
	}

	exit(0);
}
