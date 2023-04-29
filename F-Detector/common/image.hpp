#ifndef _IMAGE_HPP_
#define _IMAGE_HPP_

#include <string.h>
#include <map>

using namespace std;

typedef long unsigned int ADDRINT;


// Image sequence number type
typedef long int imgseq_t;

// Fake sequence number used when an image cannot be found
#define NOIMG_SEQNO -1

// Image structure
struct img_struct {
	ADDRINT addr, end;
	string name;
	imgseq_t seqno;
	string sha1;
	inline img_struct& operator=( const img_struct& other) ;

};

// img type
typedef struct img_struct img_t;

// A mapping from the image ID to an an image
typedef map<imgseq_t, img_t> img_map_t;

// An iterator of the img_map_t type
typedef map<imgseq_t, img_t>::iterator img_map_it;

// A mapping from the image end address to an an image
typedef map<ADDRINT, img_t> img_map_end_t;

// A mapping from the image end address to an an image
typedef map<ADDRINT, img_t>::iterator img_map_end_it;

// A mapping from the program name to its loaded images
typedef map<string, img_map_t> img_reg_t; 

// An iter over the mapping from the program name to its loaded images
typedef map<string, img_map_t>::iterator img_reg_it; 

typedef pair<img_map_end_it, bool> img_map_r_t;;

inline struct img_struct& img_struct::operator=( const struct img_struct& other) {
	if(this != &other){
		this->addr = other.addr ;
		this->end = other.end ;
		this->seqno = other.seqno ;
		this->name = other.name ;
		this->sha1 = other.sha1 ;
	}
	return *this;
}


#endif
