#ifndef __STATIC__
#define __STATIC__

#include <iostream> 

#include "image.hpp"

#define XSTR(s) STR(s)
#define STR(s) #s

// Path to ida cli executable
#ifndef IDA
#error "IDA executable is not defined"
#endif
#define IDA_PATH  XSTR(IDA)
// #ifdef DEBUG_IDA
// #define IDA_PATH IDA_EXE
// #else
// #define IDA_PATH IDA_T_EXE
// #endif

// path to idapython script
#define IDA_SCRIPT "-S\"/feat_removal\
/identification/tools/ida_static_engine.py\""

#define STDOUT 1
#define STDERROR 2


// Commands sent to the static analysor

// start the static analysis
// usage: START
#define START  0xCAFED00D
// end the static analysis
// usage: END
#define END  0xDEADBEEF
// get type of the branch from this block
// usage: TYPE BBL_ADDRESS
#define	TYPE  0x01010101
// get the number of data references to the specified address
// usage: DREFS BBL_ADDRESS
#define	DREFS  0x02020202
// get the number of code references to the specified address
// usage: CREFS BBL_ADDRESS
#define	CREFS  0x03030303
// // check if their is a code reference to the edge destination different  
// // than the edge source
// // usage: XREF BBL_SRC_ADDRESS BBL_DST_ADDRESS 
// #define XREFS  0x04040404
// Check if the current function returns void
// usage: VRET BBL_ADDRESS
#define	VRET  0x05050505
// Get boundof function
// usage: FEND BBL_ADDRESS
#define	FEND  0x06060606
// Get name of function
// usage: FNAME BBL_ADDRESS
#define	FNAM  0x07070707

// Types of branches 

// indirect call
#define ICALL  0
// direct call
#define CALL  1
// indirect jump
#define IJMP  2
// direct jump
#define JMP  3
// return
#define RET  4
// coditional jump
#define CJMP  5
// unkown
#define NA  -1
// error
#define ERROR  -99

// A flag to put in edges
#define EDG_FLAG 0x00000000

// An error wrapped in an edge
#define EDG_ERROR 0xffffffff

// IDA returning erro
#define IDA_ERROR 0x0F0F0F0F

// ISVOID return code

// The function returns a non void value
#define FNONVOID 1
// The function returns a void value
#define FVOID 0
// The function returns a non void value but never used
#define FNONVOIDWARN 2


// command type
typedef uint32_t cmnd_t;

// branch type type
typedef int b_type_t;

class PIPE
{
public:
	PIPE() {};
	PIPE(string pipe, string image);
	PIPE(const PIPE &obj);
	~PIPE();

	// send in pipe
	bool send(int num, ...);
	// recieve from pipe
	bool rcv(int num, ...);
	// for removing the lock in the sender
	bool rcv();

	bool ok = false;

private: 
	// path to the pipe file
	string _pname;
	// path to the image file
	string _iname;

	static vector<string> _open;
};

namespace SE {

	class SE
	{
	public:
		SE () {};
		SE(imgseq_t seq);
		SE(const SE &obj);
		~SE();

		b_type_t get_branch_type(ADDRINT addr);
		int get_data_refs(ADDRINT addr);
		int get_code_refs(ADDRINT addr);
		bool is_nonvoid_ret(ADDRINT addr);
		pair<ADDRINT,ADDRINT> get_func_bound(ADDRINT addr);
		string get_func_name(ADDRINT addr);
		inline const bool is_main_exec() {return imgseqno == 0;};
		bool initialized = false;

		// image sequence number
		imgseq_t imgseqno;
		// image file path
		string imgfile;
		// username; used to create a directory for each user in /tmp
		string username;
		// pipe to send commands
		// holds all statically analysed images
		static vector<imgseq_t> init_images;
		// information of the loaded images
		static img_map_t imgs;

	private:
		string build_cpipe();
		string build_rpipe();
		PIPE _cmnd;
		// pipe to receive responses
		PIPE _resp;

	};
#ifndef WITHPIN
	string get_func_name(string imgfile, ADDRINT);
	bool exist_in_reloc(string imgfile, string funcname);
	ADDRINT name_to_addr_reloc(string imgfile, string funcname);
#endif	
}



#endif
