#include <iostream> 
#include <dirent.h> 
#include <fstream>
#include <string.h>
#include <map>
#include <vector>
#include <sys/stat.h>
#include <iomanip>
#include <utility>

#include "common.hpp"

using namespace std; 

// map of all threads of each program execution
typedef	map<string,map<TID_t,string>> thread_map_t;
typedef	map<string,map<TID_t,string>>::iterator thread_map_it;


string get_base(const char* file){
	const char* p = strstr(file, "_thread");
	int size = 0;
	if(p)
	 	size = p - file;
	if(size <= 0)
		return string();

	char result[40]; 
	strncpy(result, file, size>39?39:size);
	result[size>39?39:size] = 0;
	return string(result);


}

int get_nb(const char* file){
	const char* p = strstr(file, "_thread");
	if(p)
		return atoi(p+7);
	else
		return -1;


}

int main(int argc, char* argv[]){
	if(argc != 2){
		cerr << "Usage: " << argv[0] << " <dir> " << endl;
		exit(-1);
	}

	string base_path = string(argv[1]);
	if(base_path[base_path.size() - 1] != '/')
		base_path += "/";
	string path = base_path + "threads";

	DIR* dir;
	dirent* pdir;
	dir = opendir(path.c_str());
	if(!dir){
		cerr << "invalid directory! " << path << endl; 
		exit(-1);
	}

	thread_map_t thread_map;

	while ((pdir = readdir(dir))) {
		if(!strcmp(pdir->d_name,".") || !strcmp(pdir->d_name,".."))
			continue;

		string thread_file =path+ "/" + string(pdir->d_name);

		string base = get_base(pdir->d_name);
		int th_nb = get_nb(pdir->d_name);
		if(th_nb >= 0)
			thread_map[base][th_nb] = thread_file;
	}
	closedir(dir);
	int cn =0;

	cout << "[-] Merging threads belonging to the same execution: 0/" << thread_map.size() << "\r";
	cout.flush();

	path = base_path + "/profiles";
	
	dir = opendir(path.c_str());
	if (dir) {
	    closedir(dir);
	} else if (ENOENT == errno) {
		mkdir(path.c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
	} 

	for(thread_map_it it=thread_map.begin(); it != thread_map.end(); it++){
		path = base_path + "/profiles";
		
		// cout << "MERGING: " << it->first << endl;
		
		edg_map_t edges;

		for (map<int,string>::iterator th = it->second.begin();
				th!=it->second.end(); th++){

			ifstream ifs(th->second, ifstream::binary);
			while (!ifs.eof()){

				pair<POS_t,LOOP_t> value; 
				edg_t e;
				ifs.read((char*)&value, sizeof(pair<POS_t,LOOP_t>));
				ifs.read((char*)&e,sizeof(edg_t));
				edg_map_it edg_find = edges.find(e);
				if(edg_find == edges.end()){
					edges[e].pos = value.first;
					edges[e].loops = value.second;
					edges[e].tid = th->first;
				}
				/*else{
					if (get<1>(edges[e]) > get<1>(edg_find->second))
						get<1>(edges[e]) = get<1>(edg_find->second);
				}*/
			}

		}
		ofstream os(path + "/" + it->first, ofstream::binary);
		for(edg_map_it e_it = edges.begin();
				e_it != edges.end(); e_it++){

			os.write((char*)&e_it->second,sizeof(edg_info_t));
			os.write((char*)&e_it->first,sizeof(edg_t));
		// os << setw(2) << setfill('0') << e_it->second.tid << " " <<
		// 	setw(10) << setfill('0') << e_it->second.pos << " " <<
		// 	setw(3) << setfill('0') << e_it->first.imgsrc << ":" <<
		//  	hex << "0x" << setw(8) << setfill('0') << e_it->first.src << dec << " " <<
		// 	setw(3) << setfill('0') << e_it->first.imgdst << ":" << 
		// 	hex << "0x" << setw(8) << setfill('0') << e_it->first.dst << dec << endl;

		}

		cout << "[-] Merging threads belonging to the same execution: " << ++cn << "/" << thread_map.size() << "\r";
		cout.flush();
	}
	cout << endl;
	return 0;

}
