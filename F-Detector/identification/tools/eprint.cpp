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
#include "utils.hpp"


int 
main(int argc, char* argv[])
{

		ifstream ifs(argv[1], ifstream::binary);
		if(!ifs.is_open())
			return 0;
		while (!ifs.eof()){
			edg_info_t pos; 
			edg_t e;
			ifs.read((char*)&pos, sizeof(edg_info_t));
			ifs.read((char*)&e,sizeof(edg_t));
            cout <<
		setw(2) << setfill('0') << pos.tid << " " <<
			setw(10) << setfill('0') << pos.pos << " " <<
			setw(2) << setfill('0') << e.imgsrc << ":" <<
		 	hex << "0x" << setw(8) << setfill('0') << e.src <<
		 	dec << " " << setw(2) << setfill('0') << e.imgdst << 
		 	":" << hex << "0x" << setw(8) << setfill('0') <<
		 	e.dst << dec << " " << pos.loops << endl;

		}
	return 1;
	}


