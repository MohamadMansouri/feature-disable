#include <iostream> 
#include <map>
#include <vector>
#include <string.h>
#include <unistd.h>
#include <assert.h>
#include <fcntl.h>
#include <fstream>

#include "image.hpp"
#include "putils.hpp"

// store the stdout and stderror for later
static int term_stdout = dup(STDOUT);
static int term_stderror = dup(STDERROR);
static int null = open("/dev/null", O_RDWR);
static ofstream* odbg;

bool load_img_registry(string path, img_map_t &img_registry)
{
	 string imgfile = path + "IMAGES";

	if(FILE *fp = fopen(imgfile.c_str(), "r")){	
		char* line = NULL;
		size_t len = 0;
	    while (getline(&line, &len, fp) != -1) {
			img_t img_reg;
			char tmpname[100], tmpsha[SHA_DIGEST_LENGTH*2 +1];
			sscanf(line, "%ld %lx %lx # %s %s", &img_reg.seqno, &img_reg.addr, &img_reg.end, tmpsha, tmpname);
			img_reg.name = string(tmpname);
			img_reg.sha1 = string(tmpsha);
			img_registry[img_reg.seqno] = img_reg;
		}
		return true;
	}

	return false;
}

imgseq_t imgseqno_lookup(string img_name, img_map_t img_registry) 
{
	// cout << img_name << endl;
	img_map_it it = img_registry.begin();
	for (; it != img_registry.end(); ++it){
		if (it->second.name == img_name)
			return it->first;
	}
	return NOIMG_SEQNO;
}

void silence() 
{
#ifndef DEBUG
	assert(null != -1);
	dup2(null, STDOUT);
	dup2(null, STDERROR);
#endif
}

void unsilence() 
{
#ifndef DEBUG
	dup2(term_stdout, STDOUT);
	dup2(term_stderror, STDERROR);			
#endif
}

void INITDEBUG(ofstream* ofs)
{
	odbg = ofs;
}

void MDEBUG(string msg)
{
#ifdef DEBUG	
	assert(odbg);
	odbg->write(msg.c_str(), msg.length());
	odbg->write("\n", 1);
	odbg->flush();
	// *odbg << msg << endl;
#endif	
}