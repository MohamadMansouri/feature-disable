#include "pin.H"
#include "instlib.H"
#include "image.hpp"
#include "common.hpp"
#include "putils.hpp"
#include "sideband.hpp"
// #include "shared-mem.hpp"

#include <string>
#include <set>
#include <sys/types.h>
#include <assert.h>
#include <fcntl.h>
#include <sys/syscall.h>
#include <unistd.h>

#define likely(x)       __builtin_expect((x),1)
#define unlikely(x)     __builtin_expect((x),0)


#define BUF_SLOTS 1


struct bbl_window {
	int cur;
	bbl_t prev_bbl;
	// bbl_t buf[BUF_SLOTS];
	bbl_t curr_bbl;
};

// PIN REG to store the basic blocks of a thread
static REG bfreg;

// vector holding the possible identified edges
static vector<edg_t> iedg;

// map of images 
static img_map_t img_registry;

// currently tested edge
static edg_t edge;

// the path to the profiled program
static string inpath;

// the path to the output
static string outpath;

// the cache of already synced images
static map<imgseq_t, imgseq_t> imgsync_cache;

// debug, idetified edge and callstack files
static ofstream odbg, iefs;

// Pintool options
static KNOB<string> indir(KNOB_MODE_WRITEONCE, "pintool",
	"d","", "the directory of the profiled program");

static KNOB<string> outdir(KNOB_MODE_WRITEONCE, "pintool",
	"o","<same as profile dir>", "the directory of the profiled program");

// store the stack frames while the program executes
// static CS call_stack(Target2RtnName, Target2LibName);

#if FORK > 0
static pid_t child_pid = -1;
static int forks = 0;
#endif

/****************************************************************
*********************** GENERAL UTILITIES ***********************
****************************************************************/

imgseq_t sync_imgno(imgseq_t seqno)
{	
	map<imgseq_t, imgseq_t>::iterator i = imgsync_cache.find(seqno);

	if (i != imgsync_cache.end())
		return i->second;

	string img_name = sideband_find_imgname(seqno);
	imgseq_t s = imgseqno_lookup(img_name, img_registry);
	img_registry[s].addr = sideband_find_imgaddr(seqno);
	imgsync_cache[seqno] = s;
	assert(s != NOIMG_SEQNO);
	return s;
}

void save_identified_edg(edg_t e) 
{
	assert(iefs.is_open());
	iefs << hex << "0x" << e.src << " " << dec << e.imgsrc << endl;
	iefs << hex << "0x" << e.dst << " " << dec << e.imgdst << endl;
	// iefs.write((char*)&e, sizeof(edg_t));
}

void check_edg(edg_t edge)
{
	
	ostringstream oss;
	if (iedg[0].src == edge.src)
	{
		oss << " src bbl is reached dst = @ 0x" << hex << edge.dst;
		MDEBUG(oss.str());
	}
	else if ( iedg[0].dst  == edge.dst )
	{
		oss.clear();
		oss.str("");
		oss << "dst bbl is reached src = @ 0x" << hex << edge.src;
		MDEBUG(oss.str()); 
	}
		
	for (uint32_t i=0; i< iedg.size(); ++i)
	{
		unsilence();
		if (iedg[i] == edge) {
			save_identified_edg(edge);
		 	cout << "[-] The detected entrace is:" << endl;

			cout << "\tSource BBL:      " << hex << "0x" << setw(8) 
				 << setfill('0') << edge.src << "\tin image: " 
				 << img_registry[edge.imgsrc].name << endl

				 << "\tDestination BBL: " << hex << "0x" << setw(8) 
				 << setfill('0') << edge.dst << "\tin image: " 
				 << img_registry[edge.imgdst].name << endl;
			oss.clear();
			oss.str("");
			oss << "found feature entrance @ 0x" << hex << edge.dst;
			MDEBUG(oss.str()); 
			PIN_ExitApplication(0);
		}
	}
	// silence();
}



/****************************************************************
********************* END GENERAL UTILITIES *********************
****************************************************************/

/****************************************************************
*********************** ANALYSIS CALLBACKS **********************
****************************************************************/

// static PIN_FAST_ANALYSIS_CALL ADDRINT if_buf_full(struct bbl_window* bf)
// {
// 	return (ADDRINT)(bf->cur >= BUF_SLOTS);
// }

// static PIN_FAST_ANALYSIS_CALL VOID search_buf(struct bbl_window* bf, CS* cs)
// {
// 	for (int i=0; i < bf->cur ; i++)
// 	{
// 		check_bbl(edg_t(bf->prev_bbl, bf->buf[i]), cs);
// 		bf->prev_bbl = bf->buf[i];
// 	}
// 	bf->cur = 0;
// }

static PIN_FAST_ANALYSIS_CALL VOID identify_edg(struct bbl_window* bf,
		UINT32 img, ADDRINT addr)
{
	bf->prev_bbl = bf->curr_bbl;
	bf->curr_bbl.imgno = (imgseq_t)img;
	bf->curr_bbl.addr = addr;
	check_edg(edg_t(bf->prev_bbl, bf->curr_bbl));
}

static PIN_FAST_ANALYSIS_CALL ADDRINT is_this_main(ADDRINT addr)
{
	return (ADDRINT)(addr == main_address);
}

// No need to lock the shared variable main_address
// (one threads exist before main)
static PIN_FAST_ANALYSIS_CALL VOID do_main_seen(ADDRINT target, ADDRINT sp)
{
	ostringstream oss;
	oss << "Main found @ 0x" << hex <<target;	
	MDEBUG(oss.str());

	main_address = ADDR_SEEN;
}

static PIN_FAST_ANALYSIS_CALL ADDRINT is_main_seen()
{
	return (ADDRINT)(main_address == ADDR_SEEN 
#if FORK > 0
		&& PIN_GetPid() == child_pid && forks == FORK
#endif
		);
}

/****************************************************************
********************* END ANALYSIS CALLBACKS ********************
****************************************************************/

/****************************************************************
******************* INSTRUMENTATION UTILITIES *******************
****************************************************************/

static inline VOID bbl_ins(TRACE trace, BBL bbl, vector<BBL>* stacked,
						   bool resolve)
{
	ADDRINT base;
	imgseq_t seqno;
	seqno = sideband_find_img(BBL_Address(bbl), &base, resolve);

	if (resolve) 
		assert(seqno != NOIMG_SEQNO);
	// keep the bbl without instrumentation
	else if (seqno == NOIMG_SEQNO)
	{
		stacked->push_back(bbl);
		return;
	}

	// sync the img seqno with the img registry
	seqno = sync_imgno(seqno);
	// Check (by analysis code) if we have seen main and start loging
	if( main_address != ADDR_SEEN && main_address != ADDR_UNKNOWN)
	{
		BBL_InsertIfCall(bbl, IPOINT_BEFORE, (AFUNPTR)is_this_main,
						 IARG_FAST_ANALYSIS_CALL,
						 IARG_ADDRINT, BBL_Address(bbl),
						 IARG_END);

		BBL_InsertThenCall(bbl, IPOINT_BEFORE, (AFUNPTR)do_main_seen,
						   IARG_FAST_ANALYSIS_CALL,
						   IARG_ADDRINT, RTN_Address(TRACE_Rtn(trace)) - base,
						   IARG_REG_VALUE, REG_STACK_PTR,
						   IARG_UINT64, (UINT64)seqno,
						   IARG_END);
		
		// check using analysis code if main is seen since bbls in the trace 
		// of the main cant see the updated value of is_main_seen 
		BBL_InsertIfCall(bbl, IPOINT_BEFORE, (AFUNPTR)is_main_seen,
						 IARG_FAST_ANALYSIS_CALL,
						 IARG_END);
		BBL_InsertThenCall(bbl, IPOINT_BEFORE, (AFUNPTR)identify_edg,
						   IARG_FAST_ANALYSIS_CALL,
						   IARG_REG_VALUE, bfreg,
						   IARG_UINT64, (UINT64)seqno,
						   IARG_ADDRINT, BBL_Address(bbl) - base,
						   IARG_END);
	}

	// if (scope_imgs.find(seqno) == scope_imgs.end() ||
	// 	(main_address == ADDR_UNKNOWN))
	// 	return;
	
	
	else //main seen, no need to insert analysis code to check for that
	{
		BBL_InsertCall(bbl, IPOINT_BEFORE, (AFUNPTR)identify_edg,
			IARG_FAST_ANALYSIS_CALL,
			IARG_REG_VALUE, bfreg,
			IARG_UINT64, (UINT32)seqno,
			IARG_ADDRINT, BBL_Address(bbl) - base,
			IARG_END);
	}
}

/****************************************************************
***************** END INSTRUMENTATION UTILITIES *****************
****************************************************************/

/****************************************************************
******************* INSTRUMENTATION CALLBACKS *******************
****************************************************************/

static VOID trace_ins(TRACE trace, VOID *v)
{
	vector<BBL> stacked;
	for (BBL bbl = TRACE_BblHead(trace); BBL_Valid(bbl); 
			bbl = BBL_Next(bbl)) {
		bbl_ins(trace, bbl, &stacked, false);
	}
	vector<BBL>::iterator it = stacked.begin(); 
	for (; it != stacked.end(); ++it)
		// Try again but this time resolve images
		bbl_ins(trace, *it, &stacked, true);
}




static VOID thread_start(THREADID tid, CONTEXT *ctx, INT32 flags, VOID *v)
{
	ostringstream oss;
	oss << "Thread " << tid << " Started"; 
	MDEBUG( oss.str());
	
	struct bbl_window *bf;

	bf = new struct bbl_window;
	assert(bf);

	bf->cur = 0;
	PIN_SetContextReg(ctx, bfreg, (ADDRINT)bf);
}

static VOID thread_fini(THREADID tid, const CONTEXT *ctx, INT32 code, VOID *v)
{
	ostringstream oss;
	oss << "Thread " << tid << " Ended"; 
	MDEBUG(oss.str());

	struct bbl_window *bf;
	// cout << "Exit thread " << tid << endl;
	bf = (struct bbl_window *)PIN_GetContextReg(ctx, bfreg);
	delete bf;
}


static VOID fini(INT32 code, VOID *v)
{ 
	MDEBUG("Program exited");
	unsilence();

}


#if FORK > 0
static VOID forkchild(THREADID tid, const CONTEXT* ctxt, VOID * arg)
{
	forks++;
	child_pid = PIN_GetPid();
}
#endif

/****************************************************************
***************** END INSTRUMENTATION CALLBACKS *****************
****************************************************************/

static VOID usage(void)
{
	cerr << KNOB_BASE::StringKnobSummary() << endl;
}

int main (int argc, char *argv[])
{
	PIN_InitSymbols();

	if (PIN_Init(argc, argv)) {
		usage();
		return EXIT_FAILURE;
	}
	inpath = indir.Value();
	if (inpath.empty()){
		puts("[x] Failed to open directory\n");
		exit(-1);
	}

	if (inpath[inpath.size() - 1] != '/')
		inpath += '/';


	string pefile = inpath + "possible";

	ifstream ifs(pefile.c_str(), ifstream::binary);
	if (!ifs.is_open()){
		puts("[x] Failed to open edges file\n");
		exit(-1);
	}

	outpath = outdir.Value();
	if (outpath == "<same as profile dir>"){
		outpath = inpath;
	}

	if (outpath[outpath.size() - 1] != '/')
		outpath += '/';

#ifdef DEBUG
	string dbgfile = outpath + "debug";
	odbg.open(dbgfile.c_str(), ios_base::app);
	if (!odbg.is_open()){
		puts("[x] Failed to open debug file\n");
		exit(-1);
	}
	INITDEBUG(&odbg);
#endif

	MDEBUG("********starting retracer********");

	edg_t e;
	ifs.read((char*)&e,sizeof(edg_t));
	ostringstream oss;
	oss << "possible edges:" << endl;
	while (!ifs.eof()){
		oss << "\tsrc = 0x" << hex << e.src << " img = " << dec << e.imgsrc << endl
			<< "\tdst = 0x" << hex << e.dst<< " img = " << dec << e.imgdst << endl;
		MDEBUG( oss.str());
		oss.str("");
		oss.clear();
		iedg.push_back(e);
		ifs.read((char*)&e,sizeof(edg_t));

	}

	// load image registry of the tracing phase
	if (!load_img_registry(inpath, img_registry)){
		puts("[x] Failed to open image registry\n");
		MDEBUG("Failed to open image registry");
		exit(-1);	
	}

	cout << "[-] Retracing the unwanted feature" << endl;

	// string csfile = path + "callstack";
	// csfs.open(csfile.c_str(), ios::out | ios::binary);
	// assert(csfs.is_open());
		
	string iefile = outpath + "identified";
	iefs.open(iefile.c_str(), ofstream::out);
	assert(iefs.is_open());
	
	// Use a Pin scratch register for the currently active sub-buffer
	bfreg = PIN_ClaimToolRegister();

	sideband_init(string());
        
    // Add instrumentation functions    
	TRACE_AddInstrumentFunction(trace_ins, NULL);
	PIN_AddThreadStartFunction(thread_start, NULL);
	PIN_AddThreadFiniFunction(thread_fini, NULL);
	PIN_AddFiniFunction(fini, 0);
#if FORK > 0
    PIN_AddForkFunction(FPOINT_AFTER_IN_CHILD, forkchild, 0);
#endif
	// silence();
	PIN_StartProgram();

	return EXIT_SUCCESS;
}
