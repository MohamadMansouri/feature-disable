#ifndef __COMMON__
#define __COMMON__

#include <iostream> 
#include <map>
#include <vector>
#include <string>

#include "image.hpp"

// Fake sequence number used when an image cannot be found
#define NOIMG_SEQNO -1
#define SHA_DIGEST_LENGTH 20


// Thread ID type
typedef int TID_t;

// Loop count type
typedef int LOOP_t;

// edge position type
typedef uint64_t POS_t;

// BBL structure
struct bbl_struct {
	imgseq_t imgno;
	ADDRINT addr;
	bool operator==(const bbl_struct& other) const;
	bool operator!=(const bbl_struct& other) const;
	bool operator<(const bbl_struct& other) const;

	bbl_struct() : imgno(NOIMG_SEQNO), addr(0x0) {};
	bbl_struct(imgseq_t i, ADDRINT a) : imgno(i), addr(a) {};
};

// Edge structure
struct edge_struct {
	ADDRINT src;
	ADDRINT dst;
	imgseq_t imgsrc; 
	imgseq_t imgdst; 
	bool operator<(const edge_struct& other) const;
	bool operator==(const edge_struct& other) const;
	struct edge_struct operator=(const edge_struct& other);

	edge_struct() {};

	edge_struct(bbl_struct src, bbl_struct dst);
	edge_struct(ADDRINT src, ADDRINT dst, imgseq_t imgsrc, imgseq_t imgdst) : 
	src(src),
	dst(dst),
	imgsrc(imgsrc),
	imgdst(imgdst) {};
	
	string tostr() const;

};

#ifdef UNIQ_VTX

// special Edge structure (comparison based on src)
struct edge_struct_src {
	ADDRINT src;
	ADDRINT dst;
	imgseq_t imgsrc; 
	imgseq_t imgdst; 
	bool operator<(const edge_struct_src& other) const;
	bool operator==(const edge_struct_src& other) const;
	
	edge_struct_src(edge_struct& other){
		this->src = other.src;
		this->dst = other.dst;
		this->imgdst = other.imgdst;
		this->imgsrc = other.imgsrc;
	}
};

// special Edge structure (comparison based on dst)
struct edge_struct_dst {
	ADDRINT src;
	ADDRINT dst;
	imgseq_t imgsrc; 
	imgseq_t imgdst; 
	bool operator<(const edge_struct_dst& other) const;
	bool operator==(const edge_struct_dst& other) const;
	
	edge_struct_dst(edge_struct& other){
		this->src = other.src;
		this->dst = other.dst;
		this->imgdst = other.imgdst;
		this->imgsrc = other.imgsrc;
	}
};

#endif

// Edge information retrieved from dynamic analysis 
struct edge_information_struct {
	TID_t tid;
	LOOP_t loops;
	POS_t pos;
	bool operator<(const edge_information_struct& other) const;
};


// BBL type
typedef struct bbl_struct bbl_t;

// Location type
typedef bbl_t loc_t;

// edge type
typedef struct edge_struct edg_t;

// edge information type
typedef struct edge_information_struct edg_info_t;

// A mapping between edges and their information
typedef map<edg_t,edg_info_t> edg_map_t;

// An iterator of the edg_map_t type
typedef map<edg_t,edg_info_t>::iterator edg_map_it;

#ifdef UNIQ_VTX
// edge (compare according to source) type
typedef struct edge_struct_src edg_src_t;

// edge (compare according to destination) type
typedef struct edge_struct_dst edg_dst_t; 

// A mapping between edges and their information
typedef map<edg_src_t,edg_info_t> edg_src_map_t;

// An iterator of the edg_src_map_t type
typedef map<edg_src_t,edg_info_t>::iterator edg_src_map_it;

// A mapping between edges and their information
typedef map<edg_dst_t,edg_info_t> edg_dst_map_t;

// An iterator of the edg_dst_map_t type
typedef map<edg_dst_t,edg_info_t>::iterator edg_dst_map_it;

#endif

// An inverse of the mapping between edges and their information
typedef map<edg_info_t, edg_t> edg_map_inv_t;

// An iterator of the edg_map_t type
typedef map<edg_info_t, edg_t>::iterator edg_map_inv_it;

// A mapping between call addresses and callee first bbl
typedef map<ADDRINT, bbl_t> callee_map_t;


const char * StripPath(const char * path);

#endif
