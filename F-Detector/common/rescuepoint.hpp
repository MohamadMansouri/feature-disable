#ifndef __RP__
#define __RP__

#include "image.hpp"
#include "common.hpp"
#include "cscommon.hpp"

enum rp_type_e
{
	PTR_RP,
	VAL_RP,
	VOID_RP,
	MAIN_RP,
	ERR_RP
};


struct rp_struct
{
	int idx;
	loc_t loc;
	loc_t loc_start;
	loc_t loc_end;
	rp_type_e type;
	bool byname;
	ret_t ret;
	ADDRINT eret;
	string fname;
	rp_struct() : idx(-1), loc(), type(ERR_RP), ret(), eret(NAS) {};

};

typedef rp_struct rp_t;

void create_rp_conf(string path, img_map_t imgs, rp_t rp, edg_t e);

extern ofstream rpfs;

#endif
