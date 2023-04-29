#include "common.hpp"
#include "image.hpp"

#include <tuple>
#include <utility>

bool bbl_struct::operator==(const bbl_struct& other) const{
	return (this->imgno == other.imgno && this->addr == other.addr);
}

bool bbl_struct::operator!=(const bbl_struct& other) const{
	return (this->imgno != other.imgno || this->addr != other.addr);
}

bool bbl_struct::operator<(const bbl_struct& other) const{
	return 
	make_pair(this->imgno, this->addr) < 
	make_pair(other.imgno, other.addr);
}


edge_struct::edge_struct(bbl_t src, bbl_t dst)
{
	this->src = src.addr; 
	this->imgsrc = src.imgno;
	this->dst = dst.addr; 
	this->imgdst = dst.imgno; 
}

bool edge_struct::operator<(const edge_struct& other) const{
	return 
	make_tuple(this->src, this->dst, this->imgsrc, this->imgdst) <
	make_tuple(other.src, other.dst, other.imgsrc, other.imgdst);
}

bool edge_struct::operator==(const edge_struct& other) const{
	return 
	make_tuple(this->src, this->dst, this->imgsrc, this->imgdst) ==
	make_tuple(other.src, other.dst, other.imgsrc, other.imgdst);
}


string edge_struct::tostr() const{
	char edghex[4*sizeof(ADDRINT)+9];
  	sprintf(edghex,"%#08lx -> %#08lx", this->src, this->dst);
	return string(edghex);
}


struct edge_struct edge_struct::operator=(const edge_struct& other){
	this->src = other.src;
	this->dst = other.dst;
	this->imgsrc = other.imgsrc;
	this->imgdst = other.imgdst;
	return *this;
}

bool edge_information_struct::operator<(const edge_information_struct& other) const { return 
	make_tuple(this->tid, this->pos, this->loops) <
	make_tuple(other.tid, other.pos, other.loops);
}

#if UNIQ_VTX == 1

bool edge_struct_src::operator<(const edge_struct_src& other) const{
	return 
		make_pair(this->src, this->imgsrc) <
		make_pair(other.src, other.imgsrc)	;
}



bool edge_struct_dst::operator<(const edge_struct_dst& other) const{
	return 
		make_pair(this->dst, this->imgdst) <
		make_pair(other.dst, other.imgdst)	;
}


#endif

// Strip the name from the full path
const char * StripPath(const char * path)
{
    const char * file = strrchr(path,'/');
    if (file)
        return file+1;
    else
        return path;
}