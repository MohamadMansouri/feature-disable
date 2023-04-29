#include "pin.H"
#include "rescuepoint.hpp"
#include "putils.hpp"
#include "common.hpp"
#include "image.hpp"

#include <iostream>


void create_rp_conf(string path, img_map_t imgs, rp_t rp, edg_t e)
{
	// create the RP.txt config file
	MDEBUG("Saved RP configuration file:");
	unsilence();
	cout << "[-] Found a valid Rescue Point:" << endl
		 << "\tAddress = 0x" << hex << rp.loc.addr << endl 
		 << "\tImage = " << dec << rp.loc.imgno << endl 
		 << "\tERROR_RET = " << hex << rp.eret << endl; 
	silence();
	ostringstream oss;
	if(!rp.byname) {
		oss << "RELA\t" << StripPath(imgs[rp.loc_start.imgno].name.c_str()) << "+" 
			<< hex << rp.loc_start.addr << ":" << rp.loc_end.addr;
	}
	else {
		oss << "SYM\t" << rp.fname;
	}

	oss << "\tV\t" << dec << (int)rp.eret <<  "\tIgnoreOthers\t" 
		<< StripPath(imgs[e.imgdst].name.c_str()) << "+" << hex << e.dst;
	MDEBUG(oss.str());
	rpfs << oss.str();
	rpfs.close();
}
