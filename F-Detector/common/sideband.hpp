#ifndef _SIDEBAND_HPP_
#define _SIDEBAND_HPP_
#include "image.hpp"

#define ADDR_UNKNOWN 0xdeadbeef
#define ADDR_SEEN 0xcafed00d

// Address of the main function
extern ADDRINT main_address;

int sideband_init(string name);
void sideband_cleanup(void);
int sideband_find_img(ADDRINT addr, ADDRINT *base, bool resolve);
string sideband_find_imgname(imgseq_t imgseqno);
ADDRINT sideband_find_imgaddr(imgseq_t imgseqno);
uint32_t sideband_find_imgcount();

#endif
