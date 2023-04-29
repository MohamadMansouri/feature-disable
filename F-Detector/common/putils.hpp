#ifndef __UTILS__
#define __UTILS__

#include <string.h>
#include <fstream>

#include "image.hpp"

#define SHA_DIGEST_LENGTH 20
#define STDOUT 1
#define STDERROR 2

bool load_img_registry(string path, img_map_t &img_registry);

imgseq_t imgseqno_lookup(string img_name, img_map_t img_registry);

void silence();

void unsilence();

void INITDEBUG(ofstream *odbg);
void MDEBUG(string msg);

#endif