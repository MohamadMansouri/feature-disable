#include "pin.H"
#include "image.hpp"
#include "sideband.hpp"
#include <fstream>
#include <map>
#include <iostream>
#include <assert.h>

// Output file stream
static ofstream sbfile;

// Monotonic image sequence #
static imgseq_t img_seqno = 0;

// List of images
static img_map_end_t imgs;

// List of images by seqno
static img_map_t imgs_registry;

// Address of the main function
ADDRINT main_address = ADDR_UNKNOWN;

static VOID add_image(img_t &i)
{
	img_map_r_t r;

	if (sbfile.is_open())
		sbfile << "I: " << dec << i.seqno << ' ' << hex << i.addr << ' '
			<< i.end << " # " <<  i.name << endl;

	//imgs.insert(pair<ADDRINT, img_t>(IMG_HighAddress(img), i));
	r = imgs.insert(pair<ADDRINT, img_t>(i.end, i));
	// assert(r.second);
	imgs_registry[i.seqno] = i;
	// cerr << "LOAD " << i.name << ' ' << hex
	// 	<< i.addr << '-' << i.end << endl;
}


static VOID image_load(IMG img, VOID *v)
{
	if ( main_address == ADDR_UNKNOWN)
	{
		RTN rtn = RTN_FindByName(img, "main");
		if (!RTN_Valid(rtn)){
			rtn = RTN_FindByName(img, "__libc_start_main");
		}

		if (RTN_Valid(rtn))
			main_address = RTN_Address(rtn);
	}

	img_t i;
	img_map_r_t r;
	char rpath[PATH_MAX];

	i.addr = IMG_LowAddress(img);
	i.end = IMG_HighAddress(img);
	i.seqno = img_seqno++;
	if (realpath(IMG_Name(img).c_str(), rpath))
		i.name = rpath;
	else
		// Could not resolve
		i.name = IMG_Name(img);

	add_image(i);
}

static VOID image_unload(IMG img, VOID *v)
{
	// cerr << "UNLOAD " << IMG_Name(img) << ' ' << hex 
	// 	<< (IMG_LowAddress(img) + IMG_SizeMapped(img)) << endl;

	img_map_end_it img_it = imgs.upper_bound(IMG_LowAddress(img));
	if (img_it != imgs.end()) {
		imgs.erase(img_it);
	}
}

static int resolve_image(ADDRINT addr, ADDRINT *base)
{
	FILE *maps;
	char line[PATH_MAX + 80], *nl;

	if (!(maps = fopen("/proc/self/maps", "r"))) {
		perror("Failed to open maps file");
		return -1;
	}

	ADDRINT start, end;
	char *pathname;
	img_t i;

	i.addr = i.seqno = -1;
	while (fgets(line, PATH_MAX + 80, maps)) {
		sscanf(line, "%lx-%lx", &start, &end);
		if (!(start <= addr && addr <= end))
			continue;

		// Image matches
		pathname = strchr(line, '/');
		// assert(pathname);
		nl = strchr(line, '\n');
		if (nl)
			*nl = '\0';

		*base = i.addr = start;
		i.end = end;
		i.seqno = img_seqno++;
		if (pathname)
			i.name = pathname;
		else{
			ostringstream oss;
			oss << i.seqno;
			i.name = "unknown " + oss.str();
		}
		add_image(i);
		break;
	}

	fclose(maps);
	return i.seqno;
}


int sideband_init(string name)
{
	if (!name.empty()) {
		sbfile.open(name.c_str());
		if (!sbfile.is_open()) {
			return -1;
		}
	}
    // Capture image loading/unloading
    IMG_AddInstrumentFunction(image_load, NULL);
    IMG_AddUnloadFunction(image_unload, NULL);

	return 0;
}

void sideband_cleanup(void)
{
	if (sbfile.is_open())
		sbfile.close();
}

int sideband_find_img(ADDRINT addr, ADDRINT *base, bool resolve)
{
	imgseq_t iseqno = NOIMG_SEQNO;
	img_map_end_it img_it = imgs.upper_bound(addr);
	BOOL unresolved_image = FALSE;

	if (img_it == imgs.end()) {
		unresolved_image = TRUE;
	} else if (img_it->second.addr <= addr && addr <= img_it->first) {
		// Found the correct image
		*base = img_it->second.addr;
		iseqno = img_it->second.seqno;
	} else {
		unresolved_image = TRUE;
	}

	if (unresolved_image && resolve) {
		// cerr << "Need to resolve address " << hex << addr << endl;
		iseqno = resolve_image(addr, base);
	}

	return iseqno;
}

string sideband_find_imgname(imgseq_t imgseqno)
{
	return imgs_registry[imgseqno].name;
}

ADDRINT sideband_find_imgaddr(imgseq_t imgseqno)
{
	return imgs_registry[imgseqno].addr;
}

uint32_t sideband_find_imgcount()
{
	return imgs_registry.size();
}
