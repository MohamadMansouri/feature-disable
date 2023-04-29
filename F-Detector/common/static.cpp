#include <iostream> 
#include <stdarg.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/syscall.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <map>
#include <vector>
#include <algorithm>
#include <dirent.h>
#include <sstream>
#include <assert.h>
#include <ftw.h>

#ifndef WITHPIN
#include "elfio/elfio.hpp"
#endif

#include "static.hpp"
#include "common.hpp"
#include "image.hpp"
#include "utils.hpp"
#include "putils.hpp"
// #include "identifier.hpp"
	
vector<string> PIPE::_open;


PIPE::PIPE(string pipe, string image) :
_pname(pipe),
_iname(image) {
	if ( -1 == access(_pname.c_str(), F_OK ) &&
	 -1 == syscall(SYS_mknod, _pname.c_str(), S_IFIFO|0666, 0)) // mkfifo() does not exist in pin crt
		return;

	if (find(_open.begin(), _open.end(), _iname) == _open.end() ) 
		_open.push_back(_iname);
	
	ok = true;
}

PIPE::PIPE(const PIPE &obj)
{
	_pname = obj._pname;
	ok = obj.ok;
	_iname = obj._iname;
}

PIPE::~PIPE(){
	ok = false;
	remove(_pname.c_str());
	// vector<string>::iterator it = find(_open.begin(), _open.end(), _iname);
	// if(it != _open.end()) {
		
	// 	char idb[100];
	// 	strcpy(idb, "/tmp/");
	// 	strncat(idb, StripPath(_iname.c_str()), 90); 
	// 	strcat(idb, ".i64");
	// 	struct stat s;
	// 	int i = 5;
	// 	while (stat(idb, &s) == -1 &&  i-- >0)
	// 		usleep(500000);
	// 	remove(idb);
	// 	_open.erase(it);
	// }
}

bool PIPE::send(int num, ...){
	va_list args;
	va_start(args, num);

	cmnd_t command = va_arg(args, cmnd_t);
	int fd = open(_pname.c_str(), O_WRONLY);
	bool rc = false;
	if (fd == -1){
		va_end(args);
		return false;
	}
	ostringstream oss;
	switch (command){
		case START:
		case END:
		{
			if ( -1 != write(fd, (char*)&command, sizeof(uint32_t)))
				rc = true;
			oss << "sent in pipe: " << hex << command  << dec;
			MDEBUG(oss.str());
			break;
		}
		case TYPE:
		case CREFS:
		case DREFS:
		case VRET:
		case FEND:
		case FNAM:
		{
			if ( -1 == write(fd, (char*)&command, sizeof(uint32_t)))
				break;

			ADDRINT bbl_address = va_arg(args, ADDRINT);
			if (-1 != write(fd, (char*)&bbl_address, sizeof(ADDRINT)))
			{
			oss.str("");
			oss.clear();
			oss << "sent in pipe: " << hex << command  << " "
				<< hex << bbl_address  << dec;
			MDEBUG(oss.str());
				rc = true;
			}
			break;
		}
		default:
			break;
	}
	close(fd);
	va_end(args);
	return rc;
}


bool PIPE::rcv(int num, ...) {
	va_list args;
	va_start(args, num);

	cmnd_t command = va_arg(args, cmnd_t);
	int fd = open(_pname.c_str(), O_RDONLY);
	bool rc = false;

	ostringstream oss;
	if (fd == -1){
		va_end(args);

		oss << "failed to rcv command";
		MDEBUG(oss.str());
		return false;
	}

	switch (command) {
	
		case START:
		case END:
		{
			uint32_t resp;
			int r = read(fd, (char*)&resp, sizeof(uint32_t));

			oss.str("");
			oss.clear();
			oss << "rcv from pipe: " << hex << resp  << dec;
			MDEBUG(oss.str());

			if(-1 == r)
				break;
			if (resp == command)
				rc = true;
			break;
		}
		case TYPE:
		case CREFS:
		case DREFS:
		case VRET:
		{
			int* resp = va_arg(args, int*);
			int r = read(fd, (char*)resp, sizeof(int));

			oss.str("");
			oss.clear();
			oss << "rcv from pipe: " << hex << *resp  << dec;
			MDEBUG(oss.str());

			if(-1 != r)
				rc = true;
			break;
		}
		case FEND:
		{
			ADDRINT* fstart = va_arg(args, ADDRINT*);
			ADDRINT* fend = va_arg(args, ADDRINT*);
			int r1 = read(fd, (char*)fstart, sizeof(ADDRINT));
			int r2 = read(fd, (char*)fend, sizeof(ADDRINT));

			oss.str("");
			oss.clear();
			oss << "rcv from pipe: " << hex << *fstart << " " << *fend  << dec;
			MDEBUG(oss.str());

			if(-1 != r1 && -1 != r2)
				rc = true;
			break;	
		}
		case FNAM:
		{
			char* fname = va_arg(args, char*);
			int r = read(fd, (char*)fname, 50);

			oss.str("");
			oss.clear();
			oss << "rcv from pipe: " << hex << string(fname)  << dec;
			MDEBUG(oss.str());

			if(-1 != r)
				rc = true;
			break;	
		}
		default:
			rc = false;
	}
	close(fd);
	va_end(args);
	return rc;
}

bool PIPE::rcv(){
	int fd = open(_pname.c_str(), O_RDONLY);

	if (fd == -1)
		return false;

	close(fd);
	return true;
}

vector<imgseq_t> SE::SE::init_images;
img_map_t SE::SE::imgs;

string SE::SE::build_cpipe() {
	string cmnd_fifo = "/tmp/";
	cmnd_fifo += username;
	cmnd_fifo += "/cmnd_";
	cmnd_fifo += StripPath(imgfile.c_str());
	remove(cmnd_fifo.c_str());	
	return cmnd_fifo;
}

string SE::SE::build_rpipe(){
	string resp_fifo = "/tmp/";
	resp_fifo += username;
	resp_fifo += "/resp_";
	resp_fifo += StripPath(imgfile.c_str());
	remove(resp_fifo.c_str());	
	return resp_fifo;
}

int remove_directory(const char *path)
{
	DIR *d = opendir(path);
	size_t path_len = strlen(path);
	int r = -1;

	if (d)
	{
		struct dirent *p;

		r = 0;

		while (!r && (p=readdir(d)))
		{
			char *buf;
			size_t len;

			/* Skip the names "." and ".." as we don't want to recurse on them. */
			if (!strcmp(p->d_name, ".") || !strcmp(p->d_name, ".."))
			{
			 continue;
			}

			len = path_len + strlen(p->d_name) + 2; 

			buf = (char*)malloc(len);

			if (buf)
			{
				snprintf(buf, len, "%s/%s", path, p->d_name);
				r = unlink(buf);
				free(buf);
			}
		}
		closedir(d);
	}
	if (!r)
	{
		r = rmdir(path);
	}
	return r;
}

SE::SE::SE(imgseq_t seq) :
 imgseqno(seq),
 imgfile(imgs[imgseqno].name),
 _cmnd(build_cpipe(), imgfile),
 _resp(build_rpipe(), imgfile)
 {

	vector<imgseq_t>::iterator img_it;

	img_it = find(init_images.begin(), init_images.end(), imgseqno);
	
	if (img_it != init_images.end()) {
		initialized = true;
		return;
	}

	if(imgs.find(imgseqno) == imgs.end())
		return;

	init_images.push_back(imgseqno);

	pid_t pid = fork();
	switch (pid) {
		case -1: 
			return;
		case 0:
		{
			DIR* dir;
			string outdbdir;
			ostringstream os, oss;
			string outdb, outdbfile;
			FILE* f;
			int term_stdout = dup(STDOUT);
			int term_stderror = dup(STDERROR);
			uid_t uid;
			int fd = open("/dev/null", O_RDWR);
			if (fd == -1)
				goto fin_thread;



			// // silent ida
#ifndef DEBUG
			dup2(fd, STDOUT);
			dup2(fd, STDERROR);
#endif
			uid = syscall(SYS_getuid); // getuid() does not exist in pin crt
			oss << "uid_" << uid; 
			username = oss.str();

			os << "/tmp/" << username;
			mkdir(os.str().c_str(),S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
			os << "/IDB_" << StripPath(imgfile.c_str()) << "/";
			outdbdir = os.str();

			if((dir = opendir(outdbdir.c_str())))
			{
				os << StripPath(imgfile.c_str()) << ".i64";
				outdbfile = os.str();
				
				if((f = fopen(outdbfile.c_str(), "r")))
				{
					fclose(f);

#ifdef DEBUG_IDA		
					execl( IDA_PATH , " ", IDA_SCRIPT, outdbfile.c_str() , NULL);
#else
					execl( IDA_PATH ," ", "-A", IDA_SCRIPT, outdbfile.c_str() , NULL);
#endif
					goto ida_failed;
				}

				if(!remove_directory(outdbdir.c_str()))
				{
					perror("cannot create IDB database (potential corrupted old db exist... delete and rerun)");
					goto ida_failed;
				}	
			}
			if(mkdir(outdbdir.c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH) == -1)
			{
				string s = "cannot create IDB directory ";
				s += outdbdir;
				perror(s.c_str());
				goto ida_failed;
			}
			else 
			{
				outdb =  string("-o") + outdbdir;
			}

#ifdef DEBUG_IDA			
			execl( IDA_PATH , "-c", outdb.c_str()/*, "-A"*/, IDA_SCRIPT, imgfile.c_str(), NULL);
#else			
			execl( IDA_PATH , "-c", outdb.c_str(), "-A", IDA_SCRIPT, imgfile.c_str(), NULL);
#endif			
ida_failed:
			dup2(term_stdout, STDOUT);
			dup2(term_stderror, STDERROR);			
			
			cout << "[!] Failed to start the static analysor engine" << endl;
			MDEBUG("failed to start SE");

			if(!_cmnd.rcv(1, START))
				goto fin_thread;

			// wait for the main thread to start listening
			sleep(1);

			// tell the main thread to finish
			_resp.send(1, END);

fin_thread:
			// failed;
			exit(-1);
		}
		default:
			break;
	}	
	
	// cout << "[-] Starting the static analysor engine ..." << endl;

	if(!_cmnd.ok || !_cmnd.send(1, START))
	{
		MDEBUG("failed to send start command");
		_cmnd.ok = false;
		_resp.ok = false;
		return;
	}

	if(!_resp.ok || !_resp.rcv(1, START))
	{
		MDEBUG("failed to receive start from SE");
		_cmnd.ok = false;
		_resp.ok = false;
		return;
	}

	cout << "[-] Analysing the code of " <<
	 imgfile << " ..." << endl;

	initialized = true;

}

SE::SE::SE(const SE &obj) :
 imgseqno(obj.imgseqno),
 imgfile(obj.imgfile),
 _cmnd(obj._cmnd),
 _resp(obj._resp) 
 {}

SE::SE::~SE(){

	if(!_cmnd.ok || !_cmnd.send(1, END))
		return;
	
	cout << "[-] Closing static engine of " <<
	 imgfile << " ..." << endl;

	if(!_resp.ok || !_resp.rcv(1, END))
		return;
}

b_type_t SE::SE::get_branch_type(ADDRINT addr) {
	if(!_cmnd.ok || !_cmnd.send(2, TYPE, addr))
		return ERROR;

	b_type_t type;
	if(!_resp.ok || !_resp.rcv(2, TYPE, &type))
		return ERROR;

	return type;
}

int SE::SE::get_data_refs(ADDRINT addr){

	if(!_cmnd.ok || !_cmnd.send(2, DREFS, addr))
		return ERROR;

	int drefs;
	if(!_resp.ok || !_resp.rcv(2, DREFS, &drefs))
		return ERROR;

	return drefs;
}

int SE::SE::get_code_refs(ADDRINT addr){

	if(!_cmnd.ok || !_cmnd.send(2, CREFS, addr))
		return ERROR;

	int crefs;
	if(!_resp.ok || !_resp.rcv(2, CREFS, &crefs))
		return ERROR;

	return crefs;
}

bool SE::SE::is_nonvoid_ret(ADDRINT addr){

	if(!_cmnd.ok || !_cmnd.send(2, VRET, addr))
		return false;

	int isvoid;
	if(!_resp.ok || !_resp.rcv(2, VRET, &isvoid))
		return false;

	ostringstream oss;
	switch(isvoid) {
		case FNONVOID:
			return true;
		case FNONVOIDWARN:
			oss << "Warning: funcion @0x" <<  hex
				<< addr << " return value is never used ... ignoring RP"; 
			MDEBUG(oss.str());
		case FVOID:
		default:
			return false;
	}
}

pair<ADDRINT,ADDRINT> SE::SE::get_func_bound(ADDRINT addr){

	if(!_cmnd.ok || !_cmnd.send(2, FEND, addr))
		return make_pair(EDG_ERROR, EDG_ERROR);

	ADDRINT fstart;
	ADDRINT fend;
	if(!_resp.ok || !_resp.rcv(3, FEND, &fstart, &fend))
		return make_pair(EDG_ERROR, EDG_ERROR);
	return make_pair(fstart, fend);
}

string SE::SE::get_func_name(ADDRINT addr){

	if(!_cmnd.ok || !_cmnd.send(2, FNAM, addr))
		return string("");

	char* fname = (char*)malloc(50);
	assert(fname);
	if(!_resp.ok || !_resp.rcv(2, FNAM, fname))
		return string("");
	string ret = string(fname);
	free(fname);
	return ret;
}
// bool check_destination_uniqueness(const char* image, ADDRINT src, ADDRINT dst){
	
// 	if(!send_in_pipe(3, image, XREFS, src, dst))
// 		return false;

// 	int unique;
// 	if(!rcv_from_pipe(3, image, XREFS, &unique))
// 		return false;

// 	return (bool)unique;
// }

#ifndef WITHPIN
string SE::get_func_name(string imgfile, ADDRINT addr) {
	ELFIO::elfio reader;
	if (!reader.load(imgfile))
		return string(); 

	ELFIO::Elf_Half sec_num = reader.sections.size();
	for ( int i = 0; i < sec_num; ++i ) {
	    ELFIO::section* psec = reader.sections[i];
	    // Check section type
	    if ( psec->get_type() == SHT_SYMTAB ||
	     psec->get_type() == SHT_DYNSYM) {
	        const ELFIO::symbol_section_accessor symbols( reader, psec );
	        for ( uint32_t j = 0; j < symbols.get_symbols_num(); ++j ) {
	            string 		  		name;
	            ELFIO::Elf64_Addr   value = EDG_ERROR;
	            ELFIO::Elf_Xword    size;
	            unsigned char 		bind;
	            unsigned char 		type;
	            ELFIO::Elf_Half     section_index;
	            unsigned char 		other;
	            
	            // Read symbol properties
	            symbols.get_symbol( j, name, value, size, bind,
	                                   type, section_index, other );
	            if (value == addr)
	            	return name;
	        }
	    }
	}
	return string();


}
ADDRINT SE::name_to_addr_reloc(string imgfile, string funcname) {
	ELFIO::elfio reader;
	if (!reader.load(imgfile)) {
#if DEBUG
		cerr << "error while reading the reloc table of " <<  imgfile << endl;
#endif				
		return EDG_ERROR;
	}

	ELFIO::Elf_Half sec_num = reader.sections.size();
	
	for ( int i = 0; i < sec_num; ++i ) {
	
		ELFIO::section* psec = reader.sections[i];
	
		if (psec->get_type() == SHT_RELA ||
		 psec->get_type() == SHT_REL ) {
	
			const ELFIO::relocation_section_accessor relocs(reader, psec) ;
	
			for (uint32_t j = 0; j < relocs.get_entries_num(); ++j) {
	
               ELFIO::Elf64_Addr	offset = EDG_ERROR;
               ELFIO::Elf64_Addr	symbolValue;
               string				symbolName;
               ELFIO::Elf_Word 		type;
               ELFIO::Elf_Sxword  	addend;
               ELFIO::Elf_Sxword  	calcValue; 
	
				//Read relocs values
				relocs.get_entry(j, offset, symbolValue, symbolName, type,
					addend, calcValue );

				if (symbolName == funcname)
					return (ADDRINT)offset;
			}
		}
	}
	
#if DEBUG
		cerr << "function " << funcname  << " not found in reloc table of " <<  imgfile << endl;
#endif				
	return EDG_ERROR;
}
bool SE::exist_in_reloc(string imgfile, string funcname) {
	return name_to_addr_reloc(imgfile, funcname) != EDG_ERROR;
}
#endif
