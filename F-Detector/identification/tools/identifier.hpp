#ifndef __TRGT__
#define __TRGT__

#include <vector>

#include "common.hpp"
#include "image.hpp"

// maximum number of consequent edges
#define MAX_EDG_PACK 10

// list of edges type
typedef vector<edg_t> edg_l_t;

// An iterator of the edg_l_t type
typedef vector<edg_t>::iterator edg_l_it;

// A list of pack of edges type
typedef vector<edg_l_t> edg_p_l_t;

// An iterator of the edg_pack_t type
typedef vector<edg_l_t>::iterator edg_p_l_it;

// images loaded by the profiled program
// extern img_map_t imgs;


#endif
