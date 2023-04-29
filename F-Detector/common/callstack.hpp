#ifndef _STACKTRACE_
#define _STACKTRACE_

#include "image.hpp"
#include "common.hpp"
#include "cscommon.hpp"
#include "rescuepoint.hpp"

#include <map>
#include <set>
#include <fstream> 

#define WTHRES 10000
#define MAX_HIS 1000000
#define MAX_STACK 10000

#define RESOLVE_NAME 0xa0a0a0a0

class Activation {
public:    
  inline act_idx_t index() const { return _index; }
  inline ADDRINT current_sp() const { return _current_sp; }
  inline ADDRINT target() const { return _target; }  
  inline imgseq_t seqno() const { return _imgseqno; }  
  inline loc_t location() const {return loc_t(_imgseqno, _target); }
  inline void set_ret(ret_t ret) { _ret = ret; }
  inline ret_t ret() const { return _ret; }

  void fill_mem();
  bool ret_in_mem();

  void add_callee(Activation act);
  void pop_callee();
  act_idx_t callee_back();
  vector <act_idx_t>::iterator callee_begin();
  vector <act_idx_t>::iterator callee_end();
  size_t callee_size();

  void add_syscall(loc_t loc);
  void add_syscall_id(ADDRINT id);
  void add_syscall_ret(ADDRINT ret);
  syscall_t syscalls_back();
  vector <syscall_t>::iterator syscalls_begin();
  vector <syscall_t>::iterator syscalls_end();
  size_t syscalls_size();

  VOID write_to_disk(ofstream* ofs);
  VOID read_from_disk(ifstream* ifs);

  Activation(act_idx_t idx, ADDRINT current_sp, loc_t loc):
    _index(idx),
    _current_sp(current_sp),
    _target(loc.addr),
    _imgseqno(loc.imgno)
  { }

  Activation() : _current_sp(~0x0)
  { }

  // Activation(Activation const& other);

  bool operator==( const Activation& a) const {
    return(location() == a.location());
  }
  bool operator!=( const Activation& a) const {
    return(!(location() == a.location()));
  }

private:
  act_idx_t _index;
  ADDRINT _current_sp;
  ADDRINT _target;
  imgseq_t _imgseqno;
  ret_t _ret;
  vector<act_idx_t> _callees;
  vector<syscall_t> _syscalls;
  mrange_t _memory;
};

typedef map<loc_t, vector<Activation>> act_map_t;
typedef map<loc_t, vector<Activation>>::iterator act_map_it;
typedef map<loc_t, vector<Activation>>::const_iterator act_map_cit;

class CS {
public:

  CS( string (*t2r)(loc_t),  string (*t2l)(imgseq_t)) :
    _main_depth(~0x0),
    _Target2RtnName(t2r),
    _Target2LibName(t2l),
    _enter_opaque_lib_entry(0x0)
  {}
  // CS(CS const& other);
  
  // ~CS();

  THREADID tid = ~0x0;  
  inline UINT64 Depth() {return _activations.size();}
  inline VOID SaveStack() { _saved_stack = _activations; }
  inline bool HasSavedStack() { return (!_saved_stack.empty()); }
  inline Activation* LastActivation() { return GetActivation(_activations.back()); }
  inline const UINT64 MainDepth() {return _main_depth;};

  VOID ProcessCall(ADDRINT current_sp, loc_t target);
  VOID ProcessMain(ADDRINT current_sp, loc_t target);
  VOID ProcessReturn(ADDRINT current_sp, ret_t ret);
  VOID ProcessSyscallLocation(loc_t loc);
  VOID ProcessSyscallEnter(ADDRINT id);
  VOID ProcessSyscallExit(ADDRINT ret);

  rp_t GetRPCandidate(img_map_t imgs, int skip = 0);
  bool GetRP(CS ocs, int idx, rp_t* rp);
  int GetRPCandidateIdx();
  bool isEq(CS cs, int idx);
  bool isActivated(loc_t loc);

  VOID DumpStack(ostream *o);
  VOID DumpCurrentStack(ostream *o);
  VOID PrintActivations(ostream *o, int a);
  VOID DumpFirstAct(ostream *o, int a);
  
  VOID  WriteToDisk(ofstream* ofs);
  bool  ReadFromDisk(ifstream* ifs);


  bool isIdxValid(act_idx_t idx);
  Activation* GetActivation(act_idx_t idx);
  Activation* GetActivation(int idx);
  act_idx_t GetIndex(Activation* act);

private:
  UINT64 _main_depth;
  string (*_Target2RtnName)(loc_t);
  string (*_Target2LibName)(imgseq_t);
  UINT64 _enter_opaque_lib_entry;
  vector<act_idx_t> _activations;
  vector<act_idx_t> _saved_stack;
  act_map_t _activations_history;

  void AddToHistory(Activation act);
  VOID CreateActivation(ADDRINT current_sp, loc_t loc);
  VOID AdjustStack(ADDRINT current_sp);

  VOID PrintActivation(ostream* o,Activation* act);
  VOID PrintCallee(ostream* o, Activation act);
  VOID PrintSyscall(ostream* o, syscall_t sc);
  VOID PrintSyscall(Activation* act, Activation* pact, ostream *o);

  bool IsInSavedStack(Activation* act);

  VOID WriteHistoryToDisk(ofstream* ofs);
  VOID WriteStackToDisk(ofstream* ofs);
  VOID WriteDepthToDisk(ofstream* ofs);
  bool ReadHistoryFromDisk(ifstream* ifs);
  bool ReadStackFromDisk(ifstream* ifs);
  VOID ReadDepthFromDisk(ifstream* ifs);

  VOID GetSyscalls(Activation* act, Activation* pact, vector<syscall_t> *syscalls);
  bool HasSyscall(Activation* act, Activation* pact);

  bool IsMain(Activation* act, int aid);
};

#endif
