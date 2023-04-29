#include "pin.H"
#include "instlib.H"
#include "sideband.hpp"
#include "image.hpp"
#include "putils.hpp"
#include "common.hpp"

#include <string>
#include <sys/types.h>
#include <dirent.h>
#include <sys/stat.h>
#include <assert.h>
#include <unistd.h>
#include <fcntl.h>
#include <algorithm>

#define likely(x)       __builtin_expect((x),1)
#define unlikely(x)     __builtin_expect((x),0)


#define BUF_SLOTS 2048


struct bbl_buffer {
	int cur;
	ofstream *out;
	bbl_t buf[BUF_SLOTS];
};



// PIN REG to store the address of the buffer
static REG bfreg;

// The output file name
static string program_name;

// output directory
static string o_dir; 

// number of spawned threads
static UINT32 nb_threads = 0;

// Pintool options
static KNOB<string> feature(KNOB_MODE_WRITEONCE, "pintool",
	"f","feature", "the name of the feature");

static KNOB<string> instance(KNOB_MODE_WRITEONCE, "pintool",
	"i","instance", "the name of the feature instance");

static KNOB<int> run(KNOB_MODE_WRITEONCE, "pintool",
	"r","0", "the number of the run of the instance");

static KNOB<string> outdir(KNOB_MODE_WRITEONCE, "pintool",
	"o","~/disk/traces/", "the output directory of the traces");

#if FORK > 0

// pid_t parent_pid;
pid_t child_pid = -1;
int forks = 0;

#endif


static PIN_FAST_ANALYSIS_CALL ADDRINT if_buf_full(struct bbl_buffer* bf)
{
	return (ADDRINT)(bf->cur >= BUF_SLOTS);
}

static PIN_FAST_ANALYSIS_CALL VOID write_buf(struct bbl_buffer* bf)
{
	bf->out->write((char*)bf->buf,
		BUF_SLOTS * sizeof(bbl_t));
	bf->out->flush();
	bf->cur = 0;
}

static PIN_FAST_ANALYSIS_CALL VOID log_bbl(struct bbl_buffer* bf,
		UINT64 img, ADDRINT addr)
{
	bf->buf[bf->cur].imgno = (imgseq_t)img;
	bf->buf[bf->cur++].addr = addr;
}

static PIN_FAST_ANALYSIS_CALL ADDRINT is_this_main(ADDRINT addr)
{
		return (ADDRINT)(addr == main_address);
}

static PIN_FAST_ANALYSIS_CALL VOID start_recording()
{
	main_address = ADDR_SEEN;
}

static PIN_FAST_ANALYSIS_CALL void is_main_seen(struct bbl_buffer* bf,
		UINT64 img, ADDRINT addr)
{
	if(main_address == ADDR_SEEN
#if FORK > 0
		&& PIN_GetPid() == child_pid && forks == FORK
#endif
		)
	{
		if(bf->cur >= BUF_SLOTS)
		{
			bf->out->write((char*)bf->buf,
				BUF_SLOTS * sizeof(bbl_t));
			bf->out->flush();
			bf->cur = 0;
		} 
		bf->buf[bf->cur].imgno = (imgseq_t)img;
		bf->buf[bf->cur++].addr = addr;
	}
}

static inline VOID bbl_ins(BBL bbl, vector<BBL>* stacked, bool resolve)
{
	ADDRINT base;
	imgseq_t seqno;
	seqno = sideband_find_img(BBL_Address(bbl), &base, resolve);

	if (resolve) 
		assert(seqno >= 0);
	// keep the bbl without instrumentation
	else if (seqno == NOIMG_SEQNO)
	{
		stacked->push_back(bbl);
		return;
	}

	// Check if we reached main

	if(main_address != ADDR_UNKNOWN && main_address != ADDR_SEEN)
	{
		BBL_InsertIfCall(bbl, IPOINT_BEFORE, (AFUNPTR)is_this_main,
			IARG_FAST_ANALYSIS_CALL,
			IARG_ADDRINT, BBL_Address(bbl),
			IARG_END);
		BBL_InsertThenCall(bbl, IPOINT_BEFORE, (AFUNPTR)start_recording,
			IARG_FAST_ANALYSIS_CALL,
			IARG_END);

		// Check (by instrumentation code) if we have seen main and start loging
		BBL_InsertCall(bbl, IPOINT_BEFORE, (AFUNPTR)is_main_seen,
				IARG_FAST_ANALYSIS_CALL,
				IARG_REG_VALUE, bfreg,
				IARG_UINT64, (UINT64)seqno,
				IARG_ADDRINT, BBL_Address(bbl) - base,
				IARG_END);
	}

		if (seqno == (imgseq_t)7 && BBL_Address(bbl) - base == (ADDRINT)0x001c74f0)
			cout << hex << " 0x" << BBL_Address(bbl) << " 0x"  << base << endl;

	// check (without instrumentation) if we have seen main
	if(main_address == ADDR_SEEN)
	{
		// Check if the buffer is full
		BBL_InsertIfCall(bbl, IPOINT_BEFORE, (AFUNPTR)if_buf_full,
				IARG_FAST_ANALYSIS_CALL,
				IARG_REG_VALUE, bfreg, 
				IARG_END);
		BBL_InsertThenCall(bbl, IPOINT_BEFORE, (AFUNPTR)write_buf,
				IARG_FAST_ANALYSIS_CALL,
				IARG_REG_VALUE, bfreg, 
				IARG_END);

		BBL_InsertCall(bbl, IPOINT_BEFORE, (AFUNPTR)log_bbl,
				IARG_FAST_ANALYSIS_CALL,
				IARG_REG_VALUE, bfreg,
				IARG_UINT64, (UINT64)seqno,
				IARG_ADDRINT, BBL_Address(bbl) - base,
				IARG_END);
	}
}


static VOID trace_ins(TRACE trace, VOID *v)
{
	vector<BBL> stacked;
	for (BBL bbl = TRACE_BblHead(trace); BBL_Valid(bbl); 
			bbl = BBL_Next(bbl)) {
		bbl_ins(bbl, &stacked, true);
	}
	// vector<BBL>::iterator it = stacked.begin(); 
	// for (; it != stacked.end(); ++it)
	// 	// Try again but this time resolve images
	// 	bbl_ins(*it, &stacked, true);
}





static inline struct bbl_buffer *bf_alloc(THREADID tid)
{
	struct bbl_buffer *bf;

	bf = new struct bbl_buffer;
	assert(bf);
  	
  	// thread_ids.push_back(tid);

    ostringstream fn;
    fn << o_dir << program_name << "_" << 
	feature.Value() << "_" << instance.Value() << "_" 
	<< run.Value() << "_thread" << tid;
	
	ofstream *out = new ofstream(fn.str().c_str());

	assert(out);
	assert(out->is_open());

	bf->out = out;
	bf->cur = 0;
	
	return bf;
}

static VOID bf_free(struct bbl_buffer* bf)
{
	if (bf->cur){
		bf->out->write((char*)bf->buf,
			bf->cur * sizeof(bbl_t));
		bf->out->flush();
	}

	bf->out->close();
	delete bf->out;
	delete bf;
}



static VOID thread_start(THREADID tid, CONTEXT *ctx, INT32 flags, VOID *v)
{
	struct bbl_buffer *bf;

	// cout << "New thread " << tid << endl;
	bf = bf_alloc(tid);
	

	PIN_SetContextReg(ctx, bfreg, (ADDRINT)bf);

}

static VOID thread_fini(THREADID tid, const CONTEXT *ctx, INT32 code, VOID *v)
{
	struct bbl_buffer *bf;

	// cout << "Exit thread " << tid << endl;
	nb_threads++;
	bf = (struct bbl_buffer *)PIN_GetContextReg(ctx, bfreg);
	
	bf_free(bf);

}


static VOID usage(void)
{
	cerr << KNOB_BASE::StringKnobSummary() << endl;
}


static VOID fini(INT32 code, VOID *v)
{ 
	sideband_cleanup();
	unsilence();
	// cout << "Finished "<< endl
	// 	 << "\tNB Threads =        " << nb_threads << endl
	// 	 << "\tNB Loaded Images =  "<< get_imgs_count()
	// 	 << endl;

}

// static VOID fork(THREADID tid, const CONTEXT* ctxt, VOID * arg)
// {
// 	forks++;
// 	parent_pid = PIN_GetPid();
// }

#if FORK > 0
static VOID forkchild(THREADID tid, const CONTEXT* ctxt, VOID * arg)
{
	forks++;
	child_pid = PIN_GetPid();
}
#endif

int main (int argc, char *argv[])
{
	PIN_InitSymbols();


	if (PIN_Init(argc, argv)) {
		usage();
		return EXIT_FAILURE;
	}
	
	bool flag = false; 
	int i = 0;
	for (i = 0 ; i <  argc; ++i) {
		if(string(argv[i]).compare("--") == 0 && !flag) {
			flag = true;
			continue;
		}
		if (flag){
			program_name = StripPath(argv[i]);
			break;
		}
	}

	cout << "[-] Tracing : ";
	cout << StripPath(argv[i]) << " ";
	for (int j = i+1; j < argc ; ++j)
		cout << argv[j] << " ";
	cout << endl; //"... ";


	// Use a Pin scratch register for the currently active sub-buffer
	bfreg = PIN_ClaimToolRegister();
    

 	DIR* dir_tmp;
	flag = false;
	do{
		if(flag)
			closedir(dir_tmp);
		ostringstream os(outdir.Value());
		if(outdir.Value()[outdir.Value().size() - 1] != '/')
			os << '/';
		os << program_name << '_' << feature.Value() <<
		 "_" << instance.Value() << "_";
		for (int i = 0; i < 5; ++i) {
			os << char('0' + (rand() %10));
		}
		os << '/';
		flag = true;
		o_dir =os.str();
		os.clear();
	}	
	while((dir_tmp = opendir(o_dir.c_str())));
	
	ostringstream in;
    in << o_dir << program_name << "_" << 
	feature.Value() << "_" << instance.Value() << "_" 
	<< run.Value() << ".image";

		
	if(mkdir(o_dir.c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH) == -1){
		perror(o_dir.c_str());
		return EXIT_FAILURE;
	}

	if (sideband_init(in.str())) {
		perror("cannot initialize sideband");
		return EXIT_FAILURE;
	}

	TRACE_AddInstrumentFunction(trace_ins, NULL);
	PIN_AddThreadStartFunction(thread_start, NULL);
	PIN_AddThreadFiniFunction(thread_fini, NULL);
    // PIN_AddForkFunction(FPOINT_BEFORE, fork, 0);
#if FORK > 0
    PIN_AddForkFunction(FPOINT_AFTER_IN_CHILD, forkchild, 0);
#endif
    
	PIN_AddFiniFunction(fini, 0);

	silence();
	PIN_StartProgram();

	return EXIT_SUCCESS;
}
