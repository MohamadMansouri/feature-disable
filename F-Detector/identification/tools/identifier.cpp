#include <iostream> 
#include <dirent.h> 
#include <fstream>
#include <string.h>
#include <vector>
#include <algorithm>
#include <sys/stat.h>
#include <iomanip>
#include <stdarg.h>
#include <fcntl.h> 
#include <unistd.h>
#include <sys/stat.h>
#include <errno.h>

#include "common.hpp"
#include "identifier.hpp"
#include "static.hpp"
#include "utils.hpp"


#define NO_IDA 0

using namespace std; 


// vector holding target feature files
static vector<string> t_feature;

// vector holding non-target feature files
static vector<string> n_feature;

// debug file
static ofstream odbg;

img_map_t imgs;

#if UNIQ_VTX == 1

// check if any of the vertices of this edge is seen before
bool is_vtx_not_seen(edg_map_t edges, edg_t e){
	for(edg_map_it it=edges.begin(); it!=edges.end(); it++){
		if( 
			(it->first.src == e.src &&
			it->first.imgsrc == e.imgsrc )
			||
			(it->first.dst == e.dst &&
			it->first.imgdst == e.imgdst)
			)
			return false;
	}
	return true;
}

#endif



// is this file correspond to the target feature
bool is_target(const char* file, const char* feature){
	uint i = 0;
	if(strlen(file) < strlen(feature))
		return false;
	while(i < strlen(feature)){
		if (*(feature+i) != *(file+i) )
			return false;
		i++;
	}
	return true;
}


void read_image_table(string path){
	string img_file = path + "IMAGES";
	if (FILE *fp = fopen(img_file.c_str(), "r")) {
		char* line = NULL;
		size_t len = 0;
	    while (getline(&line, &len, fp) != -1) {
			img_t img;
			char name[100];
			char hash[41];
			sscanf(line, "%ld %lx %lx # %s %s", &img.seqno, &img.addr, &img.end, hash, name);
			img.name = string(name);
			img.sha1 = string(hash);
			imgs[img.seqno] = img;
	    }
	    free(line);
	}
}


edg_map_inv_t filter(edg_map_inv_t sorted){
	MDEBUG("***************starting filter***************");
	MDEBUG("filtering edges");
	edg_map_inv_t filtered;

	imgseq_t libc_seqno = NOIMG_SEQNO;
	imgseq_t vdso_seqno = NOIMG_SEQNO;
	imgseq_t ld_seqno = NOIMG_SEQNO;

	for(img_map_it it = imgs.begin();
			it != imgs.end(); it++){
		if(libc_seqno == NOIMG_SEQNO
		   && !strncmp(StripPath(it->second.name.c_str()),"libc-",5))
			libc_seqno = it->first;
		
		else if(vdso_seqno == NOIMG_SEQNO
		   && !strncmp(it->second.name.c_str(),"[vdso]",6))
			vdso_seqno = it->first;
		else if(ld_seqno == NOIMG_SEQNO
		   && !strncmp(StripPath(it->second.name.c_str()),"ld-",3))
			ld_seqno = it->first;
		
	}
	ostringstream oss;
	oss << "libc seq no: " << libc_seqno; 
	MDEBUG(oss.str());
	oss.clear();
	oss.str("");	
	oss << "vdso seq no: " << vdso_seqno; 
	MDEBUG(oss.str());
	for(edg_map_inv_it e_it = sorted.begin();
			e_it != sorted.end(); e_it++){
		if(e_it->first.loops == 1 && e_it->second.imgsrc != libc_seqno 
			&& e_it->second.imgdst != libc_seqno
		       	&& e_it->second.imgdst != vdso_seqno
				   && e_it->second.imgdst != ld_seqno) 	
			filtered[e_it->first] = e_it->second;
	}
	return filtered;
}

map<TID_t, edg_p_l_t> get_consequent_edges(edg_map_inv_t filtered){
	
	MDEBUG("getting consequent edges");
	map<TID_t, edg_p_l_t> edg_threads;
	edg_p_l_t edge_packs;
	edg_l_t edg_vec;

	TID_t nomore = -1;
		
	if(filtered.empty())
		return edg_threads;

	edg_info_t edg_prev = filtered.begin()->first;
	edg_vec.push_back(filtered.begin()->second);

	edg_map_inv_it e_it = filtered.begin();
	e_it++;
	
	for(; e_it != filtered.end(); e_it++){
		
		if( nomore != e_it->first.tid && 
			e_it->first.tid == edg_prev.tid && 
			e_it->first.pos == edg_prev.pos + 1){
			edg_vec.push_back(e_it->second);
		}

		else {
			if(edg_vec.size() > 1) {
				edge_packs.push_back(edg_vec);
				if (edge_packs.size() == MAX_EDG_PACK) {
					edg_threads[edg_prev.tid] = edge_packs;
					edge_packs.clear();
					nomore = edg_prev.tid; 
				}
			}
			edg_vec.clear();
			edg_vec.push_back(e_it->second);
		
			if (e_it->first.tid != edg_prev.tid && !edge_packs.empty()) {
				edg_threads[edg_prev.tid] = edge_packs;
				edge_packs.clear();
			}
			
		}

		
		edg_prev = e_it->first;
	}

	if(edg_vec.size() > 1)
		edge_packs.push_back(edg_vec);

	if (edge_packs.size())
		edg_threads[edg_prev.tid] = edge_packs;


	return edg_threads;
}

map<TID_t, edg_t> read_edge_from_file(string inpath)
{
	string pefile = inpath + "possible";

	ifstream ifs(pefile.c_str(), ifstream::binary);
	if (!ifs.is_open()){
		puts("[x] Failed to open edges file\n");
		exit(-1);
	}
	edg_t e;
	map<TID_t, edg_t> r;

	ifs.read((char*)&e,sizeof(edg_t));
	r[0]=e;
	return r;
}



map<TID_t, edg_t> identify_edge(map<TID_t, edg_p_l_t> edge_packs){
	MDEBUG("*****************starting identifier*****************");
	SE::SE::imgs = imgs;

	edg_t edge;
	map<TID_t, edg_t> edges;

	map<imgseq_t, SE::SE*> engines;
	SE::SE *se_src;
	SE::SE *se_dst;
	
	map<TID_t, edg_p_l_t>::iterator t_it =  edge_packs.begin();

	for (; t_it != edge_packs.end(); t_it++) {
		edge.src = EDG_FLAG;
		for (edg_p_l_it it = t_it->second.begin(); it != t_it->second.end(); it++){

			
			for (edg_l_it e_it = it->begin(); e_it != it->end(); e_it++) 
			{			

				if (engines.find(e_it->imgsrc) == engines.end()) {
					engines[e_it->imgsrc] = new SE::SE(e_it->imgsrc);
				}
				
				se_src = engines[e_it->imgsrc];

				if (engines.find(e_it->imgdst) == engines.end()) {
					engines[e_it->imgdst] = new SE::SE(e_it->imgdst);
				}
				
				se_dst = engines[e_it->imgdst];


				if(!se_src->initialized)
				{
					MDEBUG("Failure in initializing the static engine for image " + imgs[se_src->imgseqno].name);
					goto save;
				}
				if(!se_dst->initialized)
				{
					MDEBUG("Failure in initializing the static engine for image " + imgs[se_dst->imgseqno].name);
					goto save;
				}

				// get the type of the branch
				b_type_t type = se_src->get_branch_type(e_it->src);
				
				if( type == ICALL) {
					MDEBUG("processing indirect call " + e_it->tostr() );
					// if the called function is in a library first we make sure that
					// none of the other images uses this function.
					// Then if the source and destination images are different we check
					// only the source since we dont care how does the library uses its
					// own functions
					
					// the called function is inside a library
/*
					if (!se_dst->is_main_exec())  {
						ostringstream oss;
						oss << "edge inside library " << imgs[e_it->imgdst].name; 
						MDEBUG(oss.str());

						// make sure none of the other loaded images call this destination
						bool skip_pack = false;

						string funcname = SE::get_func_name(se_dst->imgfile,e_it->dst);
						oss.str("");
						oss.clear();
						oss << "destination function name: " << funcname;
						MDEBUG(oss.str());

						for (img_map_it im_it = imgs.begin(); im_it != imgs.end(); im_it++) {
							
							if (im_it->first == e_it->imgsrc || im_it->first == e_it->imgdst)
								continue;
							
							// const char* ii = StripPath(im_it->second.name.c_str());

							if (!funcname.empty() && SE::exist_in_reloc(string(im_it->second.name),
								funcname)) {

								oss.str("");
								oss.clear();
								oss << "function exist in reloc of " << im_it->second.name; 
								MDEBUG(oss.str());
	
								skip_pack = true;
								break;
							}
						}

						if (skip_pack)
							break;

						ADDRINT got_addr = SE::name_to_addr_reloc(se_src->imgfile, funcname);

						oss.str("");
						oss.clear();
						oss << "GOT address: " << hex << got_addr;
						MDEBUG(oss.str());

						if (got_addr == EDG_ERROR)
							got_addr = e_it->dst;
						if (se_src->get_data_refs(got_addr) > 1)
							break;
						else {
							edge = *e_it;
							goto save;
						}

					}

					// if the called function is inside the main executable then we only
					// search for pointers in the destination (the main executable).
					// There might be a possibility that the source is in a library in
		 			// case of callbacks but still we only care about the destination
					// image
					else {
						MDEBUG("edge inside main executable");

						if (se_dst->get_data_refs(e_it->dst) + 
							se_dst->get_code_refs(e_it->dst) > 1)
							break;
						else {
							edge = *e_it;
							goto save;
						}
					}
*/
					edge = *e_it;
					MDEBUG("Edge assigned to " + e_it->tostr());
					goto save;
				}
				else if (type == IJMP)
				{	
					MDEBUG("processing indirect jump " + e_it->tostr());
					edge = *e_it;
					MDEBUG("Edge assigned to " + e_it->tostr());
					if(e_it + 1 !=  it->end())
						continue;

					goto save;
				}
				else if (type == CALL)
				{
					MDEBUG("processing direct call " + e_it->tostr());
					if(edge.src == EDG_FLAG)
						break;
					else
						goto save;
				}
				else if (type == JMP)
				{
					MDEBUG("processing unconditional jump " + e_it->tostr());
					if(e_it + 1 !=  it->end())
						continue;

					goto save;
				}
				else if (type == CJMP) {
					
					MDEBUG("processing conditional jump " + e_it->tostr());
					int crefs = se_dst->get_code_refs(e_it->dst);
					if (crefs == 9999) {
						MDEBUG("skipping edge that contain only a nop or jmp");
						continue;
					}
					if ( crefs <= 1){
						edge = *e_it;
						MDEBUG("Edge assigned to " + e_it->tostr());
						if(e_it + 1 !=  it->end())
							continue;

						goto save;
					}

					if(edge.src == EDG_FLAG)
						break;
					else
						goto save;
				}

				else {
					if ( type == RET)
						MDEBUG("processing return " + e_it->tostr());
					if(edge.src == EDG_FLAG)
						break;
					else
						goto save;
				}
			}
		}
save:
		if (edge.src != EDG_ERROR && edge.src != EDG_FLAG){
			MDEBUG("edge is identified " + edge.tostr());
			edges[t_it->first] = edge;	
		}
	}

	for (map<imgseq_t, SE::SE*>::iterator i = engines.begin(); 
		i != engines.end(); i++)
		delete i->second;
	engines.clear();
	// return edge;
	return edges;
}

// sort edges
edg_map_inv_t sort_edges(edg_map_t common) {
	MDEBUG("sorting edges");
	edg_map_inv_t sorted;
	
	for(edg_map_it e_it = common.begin(); e_it != common.end(); e_it++){
		sorted[e_it->second] = e_it->first;
	}
	return sorted;
}

edg_map_t diff_edges(string path, DIR* dir, char* target) {
	MDEBUG("*****************starting differ*****************");
	MDEBUG("diffing edges");
	dirent* pdir;
	while ((pdir = readdir(dir))) {
		if(!strcmp(pdir->d_name,".") || !strcmp(pdir->d_name,"..")
			|| !strncmp(pdir->d_name,"xxx",3))
			continue;

		string feature = path + "/" + string(pdir->d_name);

		if(is_target(pdir->d_name, target)){
			t_feature.push_back(feature);
		}
		else{
			n_feature.push_back(feature);
		}

	}
	closedir(dir);

	if(t_feature.size() == 0){
		cerr << "target feature traces not found" << endl;
		exit(-1);
	}

	
	// union all non-target traces 
	edg_map_t others;

	// cout << "    NON-TARGET: ";		
	for (vector<string>::iterator nfe = n_feature.begin();
			nfe!=n_feature.end(); nfe++){
		ifstream ifs(*nfe, ifstream::binary);
		if(!ifs.is_open())
			continue;
		// cout << StripPath(nfe->c_str()) << endl;
		// if (nfe + 1 != n_feature.end())
		// 	cout  << "                " ;		

		while (!ifs.eof()){

			edg_info_t pos; 
			edg_t e;
			ifs.read((char*)&pos, sizeof(edg_info_t));
			ifs.read((char*)&e,sizeof(edg_t));

			if(others.find(e) == others.end())
				others[e] = pos;
		}
	}

	// cout << " NT="<< n_feature.size();
	// find common target edges that didnt appear in the non-target traces
	edg_map_t common;
	edg_map_t tmp;
	int i=0;
	// cout << "    TARGET:     " ;
	for (vector<string>::iterator fe = t_feature.begin();
			fe!=t_feature.end(); fe++){

		ifstream ifs(*fe, ifstream::binary);
		if(!ifs.is_open())
			continue;
		// cout << StripPath(fe->c_str()) << endl;
		// if (fe + 1 != t_feature.end())
		// 	cout   << "                ";		

		while (!ifs.eof()){

			edg_info_t pos; 
			edg_t e;
			ifs.read((char*)&pos, sizeof(edg_info_t));

			if(ifs.eof())
				break;

			ifs.read((char*)&e,sizeof(edg_t));
			if((i == 0 || common.find(e) != common.end()) && 

#if UNIQ_VTX == 1
				is_vtx_not_seen(others,e)
#else
				others.find(e) == others.end()
#endif
			) 
				tmp[e] = pos;
		}
		i++;
		common = tmp;
		tmp.clear();

	}
	// cout << " T="<< t_feature.size();
	return common;
}

int main(int argc, char* argv[]){

	if(argc < 3 || argc > 4){
		cerr << "Usage: " << argv[0] << " <data_dir> [profile_dir] <target> " << endl;
		exit(-1);
	}
	string profpath, datapath = string(argv[1]);
	if (datapath[datapath.size() - 1] != '/')
		datapath += '/';

	if(argc == 3) {
		profpath = datapath + "profiles";
	}
	else{
		profpath = string(argv[2]);
		if (profpath[profpath.size() - 1] != '/')
			profpath += '/';
	}

	DIR* dir;
	dir = opendir(profpath.c_str());
	if(!dir){
		cerr << "invalid input directory!" << endl; 
		exit(-1);
	}
	mkdir(datapath.c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
#ifdef DEBUG
	string dbgfile = datapath + "debug";
	odbg.open(dbgfile.c_str(), ios_base::app);
	if (!odbg.is_open()){
		puts("[x] Failed to open debug file\n");
		exit(-1);
	}
	INITDEBUG(&odbg);
#endif
	// classify target and non target traces
	edg_map_t trgt_edges = diff_edges(profpath, dir, argv[argc - 1]);

	cout << "[-] Finished identifying target unique edges ... Now sorting" << endl;

	edg_map_inv_t sorted = sort_edges(trgt_edges);


	// read loaded images from file
	read_image_table(datapath);



	// write all identified edges to file
 	string target = datapath + argv[argc - 1];
	ofstream os(target, ofstream::out);
	MDEBUG("writing all unique edges to file");
	for(edg_map_inv_it e_it = sorted.begin();
			e_it != sorted.end(); e_it++){
		os << setw(2) << setfill('0') << e_it->first.tid << " " <<
			setw(10) << setfill('0') << e_it->first.pos << " " <<
			setw(2) << setfill('0') << e_it->second.imgsrc << ":" <<
		 	hex << "0x" << setw(8) << setfill('0') << e_it->second.src <<
		 	dec << " " << setw(2) << setfill('0') << e_it->second.imgdst << 
		 	":" << hex << "0x" << setw(8) << setfill('0') <<
		 	e_it->second.dst << dec << " " << e_it->first.loops << endl;

	}
	
	// filter out undesired edges 
	sorted = filter(sorted);

	map<TID_t, edg_p_l_t> edge_packs = get_consequent_edges(sorted);

	// perform static analysis to identify the right edge
	map<TID_t, edg_t> identified;
	bool empty = true;

	if (!edge_packs.empty()){
#if NO_IDA == 1
		identified = read_edge_from_file(datapath);
#else
		identified = identify_edge(edge_packs);
#endif
		if (!identified.empty()){
			empty = false;
		}
	}

	if (!empty) {
		ofstream ofs(datapath + "possible", ofstream::binary);
		cout << "[-] The possible entrances of the target feature are: " << endl;

		map<TID_t, edg_t>::iterator i_it;
		for (i_it = identified.begin(); i_it != identified.end(); i_it++) {

			 if(i_it != identified.begin())
			 	cout << endl;
			ofs.write((char*) &i_it->second, sizeof(edg_t));
			ostringstream oss; 
			oss << "\tSource BBL:      " << hex << "0x" << setw(8) 
				 << setfill('0') << i_it->second.src << "\tin image: " 
				 << imgs[i_it->second.imgsrc].name << endl

				 << "\tDestination BBL: " << hex << "0x" << setw(8) 
				 << setfill('0') << i_it->second.dst << "\tin image: " 
				 << imgs[i_it->second.imgdst].name;
			cout << oss.str() << endl;
			MDEBUG(oss.str());
		}
	} 	
	else
		MDEBUG("identifier failed");

	return 0;
} 	
    
