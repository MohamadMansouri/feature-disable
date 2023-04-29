#include <iostream>
#include <vector>
#include <fstream> 

#include "pin.H"
#include "common.hpp"
#include "image.hpp"
#include "sideband.hpp"
#include "cscommon.hpp"
#include "rescuepoint.hpp"
#include "callstack.hpp"
#include "putils.hpp"
#include "static.hpp"

// instance of a an activation error
static Activation ERROR_ACT = Activation();

static bool isOpaqueLib(const string& lib) {
  return !(strncmp(StripPath(lib.c_str()),"libc",4)
    && strncmp(StripPath(lib.c_str()),"ld",2));
}

static bool isOpaqueRtn(const string& rtn) {
  return !(rtn == "malloc@@GLIBC_2.0"
    && rtn.find("GLIBC") != string::npos);
}

// Activation::Activation(Activation const& other)
// {
//   _current_sp = other._current_sp;
//   _target = other._target;
//   _imgseqno = other._imgseqno;
//   _ret = other._ret;
//   _syscalls = other._syscalls;

// }

void Activation::fill_mem() 
{ 
  if (FILE *fp = fopen("/proc/self/maps", "r"))
  {
    char buf[1024];
    ADDRINT start;
    ADDRINT end;
    while (fgets(buf, sizeof(buf), fp))
    {
      sscanf(buf, "%lx-%lx %*s", &start, &end);
      _memory.push_back(make_pair(start,end));
    }

    fclose(fp);
  }
}
  
bool Activation::ret_in_mem()
{
  mrange_it it = _memory.begin();

  for (; it != _memory.end(); ++it)
  {
    if (_ret.val >= it->first && _ret.val < it->second)
      return true;
  } 
  return false;
}
void Activation::add_callee(Activation act) 
{ 
  _callees.push_back(act.index()); 
}
  
void Activation::pop_callee() 
{ 
  _callees.pop_back(); 
}

act_idx_t Activation::callee_back()
{
  return _callees.back();
}
  
vector <act_idx_t>::iterator Activation::callee_begin() 
{ 
  return _callees.begin(); 
}

vector <act_idx_t>::iterator Activation::callee_end() 
{ 
  return _callees.end(); 
}

size_t Activation::callee_size()
{
 return _callees.size(); 
}

void Activation::add_syscall(loc_t loc) 
{ 
  _syscalls.push_back(syscall_t(loc)); 
}

void Activation::add_syscall_id(ADDRINT id) 
{
  _syscalls.back().id = id; 
}

void Activation::add_syscall_ret(ADDRINT ret) 
{ 
  _syscalls.back().vret = ret; 
}

syscall_t Activation::syscalls_back() 
{ 
  return _syscalls.back(); 
}

vector <syscall_t>::iterator Activation::syscalls_begin() 
{ 
  return _syscalls.begin(); 
}

vector <syscall_t>::iterator Activation::syscalls_end() 
{ 
  return _syscalls.end(); 
}

size_t Activation::syscalls_size() 
{ 
  return _syscalls.size(); 
}

VOID Activation::write_to_disk(ofstream* ofs)
{
  if (!ofs->is_open())
  {
    cerr << "failed to dump to file" << endl;
    return;
  }
  static size_t thres =0;
  thres++;
  if((thres % WTHRES) == 0 )
    ofs->flush();
  ofs->write((char*)&_index, sizeof(act_idx_t));
  ofs->write((char*)&_current_sp, sizeof(ADDRINT));
  ofs->write((char*)&_target, sizeof(ADDRINT));
  ofs->write((char*)&_imgseqno, sizeof(imgseq_t));
  ofs->write((char*)&_ret, sizeof(ret_t));
  
  size_t s = _callees.size();
  ofs->write((char*)&s, sizeof(size_t));

  vector<act_idx_t>::iterator cit = _callees.begin();
  for(; cit != _callees.end(); ++cit)
  {
    ofs->write((char*)cit, sizeof(act_idx_t));
  }
  s = _syscalls.size();
  ofs->write((char*)&s, sizeof(size_t));
  vector<syscall_t>::iterator scit = _syscalls.begin();
  for(; scit != _syscalls.end(); ++scit)
  {
    ofs->write((char*)scit, sizeof(syscall_t));
  }

  s = _memory.size();
  ofs->write((char*)&s, sizeof(size_t));
  mrange_it mit = _memory.begin();
  for(; mit != _memory.end(); ++mit)
  {
    ofs->write((char*)mit, sizeof(pair<ADDRINT,ADDRINT>));
  }
}
VOID Activation::read_from_disk(ifstream* ifs)
{
  if (!ifs->is_open())
  {
    cerr << "failed to read from file" << endl;
    return;
  }

  ifs->read((char*)&_index, sizeof(act_idx_t));
  ifs->read((char*)&_current_sp, sizeof(ADDRINT));
  ifs->read((char*)&_target, sizeof(ADDRINT));
  ifs->read((char*)&_imgseqno, sizeof(imgseq_t));
  ifs->read((char*)&_ret, sizeof(ret_t));

  size_t s;
  ifs->read((char*)&s, sizeof(size_t));
  for (size_t i = 0; i < s; i++)
  {
    act_idx_t idx;
    ifs->read((char*)&idx, sizeof(act_idx_t));
    _callees.push_back(idx);
  }

  ifs->read((char*)&s, sizeof(size_t));
  for (size_t i = 0; i < s; i++)
  {
    syscall_t sc;
    ifs->read((char*)&sc, sizeof(syscall_t));
    _syscalls.push_back(sc);
  }

  ifs->read((char*)&s, sizeof(size_t));
  for (size_t i = 0; i < s; i++)
  {
    pair<ADDRINT,ADDRINT> mr;
    ifs->read((char*)&mr, sizeof(pair<ADDRINT, ADDRINT>));
    _memory.push_back(mr);
  }

}
bool CS::IsInSavedStack(Activation* act)
{
  vector<act_idx_t>::iterator itr = _saved_stack.begin();
  for (; itr != _saved_stack.end(); ++itr)
  {
    if (GetActivation(*itr) == act)
      return true;
  }  
  return false;

}

bool CS::isIdxValid(act_idx_t idx)
{
  if(_activations_history.find(idx.first) == _activations_history.end() ||
    idx.second < 0 ||
    idx.second >= _activations_history[idx.first].size())
    return false;
  return true;
}

Activation* CS::GetActivation(act_idx_t idx)   
{ 
  if(!isIdxValid(idx))
    return NULL;

  return &(_activations_history[idx.first][idx.second]);
}

Activation* CS::GetActivation(int idx)   
{ 
  if(idx > 0 && idx < (int)_saved_stack.size())
    return GetActivation(_saved_stack[idx]);
  return NULL;

}

void CS::AddToHistory(Activation act)
{ 
  _activations_history[act.location()].push_back(act); 
}

VOID CS::CreateActivation(ADDRINT current_sp, loc_t loc)
{
  ASSERTX(loc.imgno != NOIMG_SEQNO);

  // create new activation -- note this is sp at the callsite
  act_idx_t idx(loc, (int)_activations_history[loc].size());
  Activation new_act = Activation(idx, current_sp, loc);

  // save in history
  AddToHistory(new_act);
  
  // add as a callee of the previous activation
  if(_activations.size() > 0)
    GetActivation(_activations.back())->add_callee(new_act);

  // make it the current activation
  _activations.push_back(idx);
}

// roll back stack if we got here from a longjmp
// Note stack grows down and register stack grows up.
VOID CS::AdjustStack(ADDRINT current_sp)
{
  
  if( _activations.size() == 0 ) return;

  while( current_sp >= GetActivation(_activations.back())->current_sp() ) 
  {
    if (_activations.size() == 1) 
    {

      _activations.pop_back();
      return;
    }
    _activations.pop_back();

    ASSERTX(_activations.size() > 0);
  }
}


// standard call
VOID CS::ProcessCall(ADDRINT current_sp, loc_t target)
{
  // check if we got here from a longjmp.
  AdjustStack(current_sp);
  
    // if( _activations.size() >= _main_depth 
    //   && (_enter_opaque_lib_entry == 0 
    // || _activations.size() <= _enter_opaque_lib_entry) ) 
    // {
    // }
  ostringstream oss;

  // oss  << "[-] "
  //      << _activations.size() << "(" << _main_depth 
  //      << " " <<  _enter_opaque_lib_entry 
  //      << ") " << " :" << _Target2RtnName(target)
  //      << "@0x" << hex << target.addr
  //      << dec;
  // MDEBUG(oss.str());
  if(_activations.size() >= _main_depth 
     && !_enter_opaque_lib_entry
     && (isOpaqueLib(_Target2LibName(target.imgno))
   || isOpaqueRtn(_Target2RtnName(target))) ) {

    _enter_opaque_lib_entry = _activations.size();
  } 

  CreateActivation(current_sp, target);
}

//
// standard call
VOID CS::ProcessMain(ADDRINT current_sp, loc_t target)
{
  // check if we got here from a longjmp.
  AdjustStack(current_sp);

  _main_depth = _activations.size();
  // cerr << "Processing Main " <<endl;
  // CreateActivation(current_sp, target);
}

//
// standard return
VOID CS::ProcessReturn(ADDRINT current_sp, ret_t ret)
{
  // check if we got here from a longjmp.
  ASSERTX(_activations.size());
  // ADDRINT target = _activations.back()->target();
  // ADDRINT seqno = _activations.back()->seqno();
  // cout << dec <<_activations.size() << "( return ) :" << _Target2RtnName(target + sideband_find_imgaddr(seqno)) 
  //    << "@0x" << hex << target << dec
  //    << " ret 0x" << hex << (UINT64)val << " @0x" << addr 
  //    << " in " << _Target2LibName(imgseqno) 
  //    << endl;
  AdjustStack(current_sp);

  if( _enter_opaque_lib_entry == _activations.size() ) {
    _enter_opaque_lib_entry = 0;
  }
  if(_activations.size() == 0)
  {
    cerr << "BAD THINGS HAPPEN" << endl;
    // DumpStack(&cout);
    PIN_ExitProcess(0);
  }

  Activation* act = GetActivation(_activations.back());
  act->set_ret(ret);
  if(HasSavedStack() && IsInSavedStack(act) )
  {
    MDEBUG("Returning from a saved Activation");
    ostringstream oss;
    oss << "\tAddress = 0x" << hex << act->target() << endl
        << "\tName = " << _Target2RtnName(act->location()) << endl
        << "\tReturns = 0x" << hex << ret.val;
    MDEBUG(oss.str());
    act->fill_mem();
  }
  // pop activation
  _activations.pop_back();
}


VOID CS::ProcessSyscallLocation(loc_t loc)
{
  ASSERTX(_activations.size());
  GetActivation(_activations.back())->add_syscall(loc);
}

VOID CS::ProcessSyscallEnter(ADDRINT id)
{
  ASSERTX(_activations.size());
  GetActivation(_activations.back())->add_syscall_id(id);
}

VOID CS::ProcessSyscallExit(ADDRINT ret)
{
  ASSERTX(_activations.size());
  GetActivation(_activations.back())->add_syscall_ret(ret);
}

VOID CS::PrintActivations(ostream *o, int a)
{
  vector<act_idx_t> stack;
  if (a == 0)
    stack = _saved_stack;
  else 
    stack = _activations;

  vector<act_idx_t>::reverse_iterator i;
  size_t level = stack.size() - 1;
  for(i = stack.rbegin(); i != stack.rend(); i++) {

    *o << dec << level << ": ";
    PrintActivation(o, GetActivation(*i));
    level--;
  }
}

VOID CS::DumpFirstAct(ostream *o, int a)
{
  vector<act_idx_t> stack;
  if (a == 0)
    stack = _saved_stack;
  else 
    stack = _activations;

  vector<act_idx_t>::reverse_iterator i;
  size_t level = stack.size() - 1;
  for(i = stack.rbegin(); i != stack.rend(); i++) {

    *o << dec << level << ": ";
    PrintActivation(o, GetActivation(*i));
    if (level == stack.size() - 1)
    {
      *o << "\tCALLS :" << endl;

      vector<act_idx_t>::iterator ci = GetActivation(*i)->callee_begin();
      for (; ci != GetActivation(*i)->callee_end(); ++ci )
      {
        // check if the index is valid
        if(!isIdxValid(*ci))
          continue;
   
        *o << "\t\t";
        PrintCallee(o, _activations_history[ci->first][ci->second]);
      }

      *o << "\tSYSCALLS :" << endl;

      // vector<act_idx_t>::reverse_iterator j = i+1;
      if(i != _saved_stack.rbegin())
        PrintSyscall(GetActivation(*i), GetActivation(*(i-1)), o);
      else
        PrintSyscall(GetActivation(*i),NULL, o);
    }
    level--;
  }
}



VOID CS::DumpStack(ostream *o)
{
  vector<act_idx_t>::reverse_iterator i;
  size_t level = _saved_stack.size() - 1;
  for(i = _saved_stack.rbegin(); i != _saved_stack.rend(); i++) {

    *o << dec << level << ": ";
    PrintActivation(o, GetActivation(*i));
    *o << "\tCALLS :" << endl;

    vector<act_idx_t>::iterator ci = GetActivation(*i)->callee_begin();
    for (; ci != GetActivation(*i)->callee_end(); ++ci )
    {
      // check if the index is valid
      if(!isIdxValid(*ci))
        continue;
 
      *o << "\t\t";
      PrintCallee(o, _activations_history[ci->first][ci->second]);
    }

    *o << "\tSYSCALLS :" << endl;

    // vector<act_idx_t>::reverse_iterator j = i+1;
    if(i != _saved_stack.rbegin())
      PrintSyscall(GetActivation(*i), GetActivation(*(i-1)), o);
    else
      PrintSyscall(GetActivation(*i),NULL, o);
    level--;
  }
}


VOID CS::DumpCurrentStack(ostream *o)
{
  vector<act_idx_t>::reverse_iterator i;
  size_t level = _activations.size() - 1;
  for(i = _activations.rbegin(); i != _activations.rend(); i++) {

    *o << dec << level << ": ";
    PrintActivation(o, GetActivation(*i));
    *o << "\tCALLS :" << endl;

    vector<act_idx_t>::iterator ci = GetActivation(*i)->callee_begin();
    for (; ci != GetActivation(*i)->callee_end(); ++ci )
    {
      // check if the index is valid
      if(!isIdxValid(*ci))
        continue;
 
      *o << "\t\t";
      PrintCallee(o, _activations_history[ci->first][ci->second]);
    }

    *o << "\tSYSCALLS :" << endl;

    // vector<act_idx_t>::reverse_iterator j = i+1;
    if(i != _activations.rbegin())
      PrintSyscall(GetActivation(*i), GetActivation(*(i-1)), o);
    else
      PrintSyscall(GetActivation(*i),NULL, o);
    level--;
  }
}

VOID CS::PrintSyscall(ostream* o, syscall_t sc)
{
  *o
    << _Target2RtnName(sc.loc) 
    << hex << " @0x" << sc.loc.addr << " " 
    << "SYSCALL=0x" << sc.id << " "
    << "RETURN=0x" << sc.vret << endl;
}

VOID CS::PrintActivation(ostream* o, Activation* act)
{
    loc_t cur = act->location();
    
    *o 
       << _Target2RtnName(cur) << " @0x" << hex <<  cur.addr
       << dec << " ImgID=" << cur.imgno << " RET=0x" << hex
       << act->ret().val << endl;

}

VOID CS::PrintCallee(ostream* o, Activation act)
{
    loc_t cur = act.location();
    
    *o 
       << _Target2RtnName(cur) << " @0x" << hex <<  cur.addr
       << dec << " returns 0x" << hex << act.ret().val 
       << " @ 0x" << act.ret().loc.addr << dec << endl;

}

VOID CS::PrintSyscall(Activation* act, Activation* pact, ostream *o)
{
  vector<syscall_t>::iterator sci = act->syscalls_begin();
  for (; sci != act->syscalls_end(); ++sci )
  {
    *o << "\t\t";
    PrintSyscall(o, *sci);
  }

  vector<act_idx_t>::iterator ai = act->callee_begin();
  for (; ai != act->callee_end(); ++ai)
  {
    // if( NULL != pact)
    // {
    //   cout << hex << GetActivation(*ai)->location().addr; 
    //   PrintActivation(o, GetActivation(*ai));
    //   cout << hex << pact->location().addr; 
    //   PrintActivation(o, pact);
    //   cout << endl ;
    // }
      // cout << "found the previous activation" << endl;
    if (isIdxValid(*ai) && (pact == NULL || GetActivation(*ai) != pact))
      PrintSyscall(GetActivation(*ai), NULL, o);
  }
}
  
VOID CS::WriteHistoryToDisk(ofstream* ofs)
{
  if (!ofs->is_open())
  {
    cerr << "failed to dump to file" << endl;
    return;
  }

  size_t s = _activations_history.size();
  ofs->write((char*)&s, sizeof(size_t));
  act_map_it hit = _activations_history.begin();
  for (; hit != _activations_history.end(); ++hit)
  {
    loc_t loc = hit->first;
    s = hit->second.size();
    ofs->write((char*)&loc, sizeof(loc_t));
    ofs->write((char*)&s, sizeof(size_t));
    vector<Activation>::iterator ait = hit->second.begin();
    for (; ait != hit->second.end(); ++ait)
    {
      ait->write_to_disk(ofs);
    }
  }
  ofs->flush();
}

VOID CS::WriteStackToDisk(ofstream* ofs)
{
  if (!ofs->is_open())
  {
    cerr << "failed to dump to file" << endl;
    return;
  }

  size_t s = _saved_stack.size();
  ofs->write((char*)&s, sizeof(size_t));
  vector<act_idx_t>::iterator ssit = _saved_stack.begin();
  for(; ssit != _saved_stack.end(); ++ssit)
  {
    ofs->write((char*)ssit, sizeof(act_idx_t));
  }
  ofs->flush();
}

VOID CS::WriteDepthToDisk(ofstream* ofs)
{
  ofs->write((char*)&_main_depth, sizeof(UINT64));
  ofs->flush();
}


VOID CS::WriteToDisk(ofstream* ofs)
{
  if (!ofs->is_open())
  {
    cerr << "failed to dump to file" << endl;
    return;
  }
  WriteDepthToDisk(ofs);
  WriteHistoryToDisk(ofs);
  WriteStackToDisk(ofs);
}


bool CS::ReadHistoryFromDisk(ifstream* ifs)
{
  if (!ifs->is_open())
  {
    cerr << "failed to read from file" << endl;
    return false;
  }

  size_t s;
  ifs->read((char*)&s, sizeof(size_t));
  if(!(*ifs) || s < 0 || s > MAX_HIS)
    return false;
  for (size_t i = 0; i < s; i++)
  {
    loc_t loc;
    ifs->read((char*)&loc, sizeof(loc_t));
    size_t ss;
    ifs->read((char*)&ss, sizeof(size_t));    
    for( size_t j = 0; j < ss; ++j)
    {
      Activation act;
      act.read_from_disk(ifs);
      _activations_history[loc].push_back(act);
    }
  }
  return true;
}


bool CS::ReadStackFromDisk(ifstream* ifs)
{
  if (!ifs->is_open())
  {
    cerr << "failed to read from file" << endl;
    return false;
  }

  size_t s;
  ifs->read((char*)&s, sizeof(size_t));
  if(!(*ifs) || s < 0 || s > MAX_STACK)
    return false;

  for (size_t i = 0; i < s; i++)
  {
    act_idx_t idx;
    ifs->read((char*)&idx, sizeof(act_idx_t));
    _saved_stack.push_back(idx);
  }
  return true;
}

VOID CS::ReadDepthFromDisk(ifstream* ifs)
{
  UINT64 d;
  ifs->read((char*)&d, sizeof(UINT64));
  _main_depth = d;
}


bool CS::ReadFromDisk(ifstream* ifs)
{
  if (!ifs->is_open())
  {
    cerr << "failed to read to file" << endl;
    return false;
  }
  bool ret = false;
  ReadDepthFromDisk(ifs);
  ret = ReadHistoryFromDisk(ifs);
  ret &= ReadStackFromDisk(ifs);
  return ret;

}



VOID CS::GetSyscalls(Activation* act, Activation* pact, vector<syscall_t> *syscalls)
{
  vector<syscall_t>::iterator sci = act->syscalls_begin();
  for (; sci != act->syscalls_end(); ++sci )
    syscalls->push_back(*sci);

  vector<act_idx_t>::iterator ai = act->callee_begin();
  for (; ai != act->callee_end(); ++ai)
  {
    if (isIdxValid(*ai) && (pact == NULL || GetActivation(*ai) != pact))
      GetSyscalls(GetActivation(*ai), NULL, syscalls);
  }
}

bool CS::HasSyscall(Activation* act, Activation* pact)
{
  if(act->syscalls_size())
    return true;

  vector<act_idx_t>::iterator ai = act->callee_begin();
  for (; ai != act->callee_end(); ++ai)
  {
    if (isIdxValid(*ai) && (pact == NULL || GetActivation(*ai) != pact))
    {
      if(HasSyscall(GetActivation(*ai), NULL))
        return true;
    }
  }
  return false;
}

static map<imgseq_t, SE::SE*> engines;

static bool IsNonVoid(Activation* act, img_map_t imgs)
{
  SE::SE::imgs = imgs;

  if (engines.find(act->seqno()) == engines.end()) {
    engines[act->seqno()] = new SE::SE(act->seqno());
  }

  SE::SE *se_callee = engines[act->seqno()];
  ostringstream oss;
  oss << "checking if activation @ 0x" << hex << act->target()
      << " returns a non void value";
  MDEBUG(oss.str());
  return se_callee->is_nonvoid_ret(act->target());
}

pair<loc_t,loc_t> GetFuncBound(Activation* act, img_map_t imgs, string *fname)
{
  SE::SE::imgs = imgs;

  if (engines.find(act->seqno()) == engines.end()) {
    engines[act->seqno()] = new SE::SE(act->seqno());
  }

  SE::SE *se_callee = engines[act->seqno()];
  ostringstream oss;
  oss << "Get bounds of function of activation @ 0x" << hex << act->target();
  MDEBUG(oss.str());
  pair<ADDRINT,ADDRINT> ret = se_callee->get_func_bound(act->target());
  if(ret.first != IDA_ERROR && ret.first == ret.second)
  {
    oss.str("");
    oss.clear();
    oss << "Function in another image ... Referencing by name: ";
    *fname  = se_callee->get_func_name(act->target());
    oss << *fname ;
    MDEBUG(oss.str());
    return make_pair(loc_t(act->seqno(), RESOLVE_NAME), loc_t(act->seqno(), RESOLVE_NAME)); 

  }

  return make_pair(loc_t(act->seqno(), ret.first), loc_t(act->seqno(), ret.second)); 
}

bool IsRetPtr(Activation* act)
{
  return act->ret_in_mem();
}

bool CS::IsMain(Activation* act, int aid)
{
  return act->seqno() == 0 && (UINT64)aid + 1 == _main_depth;
}

rp_t CS::GetRPCandidate(img_map_t imgs, int skip)
{
  vector<act_idx_t>::reverse_iterator ait = _saved_stack.rbegin();
  rp_t rp;
  int aid = (int)_saved_stack.size();

  for(; ait != _saved_stack.rend(); ++ait)
  {
    aid--;
    Activation* actcur = GetActivation(*ait);
    Activation* actprv = NULL;
    
    if (ait != _saved_stack.rbegin())
      actprv = GetActivation(*(ait - 1));

    if (IsMain(actcur, aid))
    {
      rp.idx = aid;
      string fname;
      rp.byname = true;
      rp.fname = string("main");
      rp.type = MAIN_RP;
      rp.ret = actcur->ret();
      break;
    }
    else if (IsNonVoid(actcur, imgs))
    {
      if(IsRetPtr(actcur))
      {
        rp.idx = aid;
        string fname;
        pair<loc_t, loc_t> fbound = GetFuncBound(actcur, imgs, &fname);
        if (fbound.first.addr == RESOLVE_NAME)
        {
          rp.byname = true;
          rp.fname = fname;
        }
        else 
        {
          rp.byname = false;
          rp.loc_start = fbound.first;
          rp.loc_end = fbound.second;
            
        }
        rp.loc = actcur->location();
        rp.type = PTR_RP;
        rp.ret = actcur->ret();
        if(skip == 0)
          break;
        skip--;
      } 

      else if(HasSyscall(actcur, actprv))
      {
        rp.idx = aid;
        string fname;
        pair<loc_t, loc_t> fbound = GetFuncBound(actcur, imgs, &fname);
        if (fbound.first.addr == RESOLVE_NAME)
        {
          rp.byname = true;
          rp.fname = fname;
        }
        else 
        {
          rp.byname = false;
          rp.loc_start = fbound.first;
          rp.loc_end = fbound.second;
            
        }
        rp.loc = actcur->location();
        rp.type = VAL_RP;
        rp.ret = actcur->ret();
        if(skip == 0)
          break;
        skip--;
      }
    }
#ifdef VOID_RP
    else
    {
      if(HasSyscall(actcur, actprv))
      {
        rp.idx = aid;
        string fname;
        pair<loc_t, loc_t> fbound = GetFuncBound(actcur, imgs, &fname);
        if (fbound.first.addr == RESOLVE_NAME)
        {
          rp.byname = true;
          rp.fname = fname;
        }
        else 
        {
          rp.byname = false;
          rp.loc_start = fbound.first;
          rp.loc_end = fbound.second;
            
        }
        rp.loc = actcur->location();
        rp.type = VOID_RP;
        if(skip == 0)
          break;
        skip--;
      }
    } 
#endif 
  }

  for (map<imgseq_t, SE::SE*>::iterator i = engines.begin(); 
    i != engines.end(); i++)
    delete i->second;
  engines.clear();
  
  return rp;
}

bool CS::GetRP(CS ocs, int idx, rp_t* rp)
{
  MDEBUG("Checking return changes");
  
  Activation* na = GetActivation(idx);
  Activation* oa = ocs.GetActivation(idx);
  
  if (oa == NULL || na == NULL)
	{
    MDEBUG("unexpected error");
    return false;
 	}
  if (na->ret().val == NAS)
  {
    MDEBUG("failed to record a return value");
    return false;
  }

  if(na->ret().val == oa->ret().val)
    MDEBUG("failed to record a new return value, saving anyway!");
 
  ostringstream oss;
  oss << "index = " << idx
      << "\told value = 0x" << hex <<  oa->ret().val
      << "\tnew value = 0x" << hex <<  na->ret().val;
  MDEBUG(oss.str());
  rp->idx = idx;
  rp->loc = na->location();
  rp->type = VAL_RP;
  rp->ret = oa->ret();
  rp->eret = na->ret().val;
  return true;

}

int CS::GetRPCandidateIdx()
{
  int size = (int)_saved_stack.size();
  for(int i = size - 1; i >= 0; --i)
  {
    if(i != size - 1)
    {
      if(HasSyscall(GetActivation(_saved_stack[i]),
                    GetActivation(_saved_stack[i + 1])))
        return i;
    }
    else 
    {
      if(HasSyscall(GetActivation(_saved_stack[i]), NULL))
        return i;
    }
  }
  return -1;
}

bool CS::isEq(CS cs, int idx)
{
  int asize = (int)_activations.size();
  if (asize != idx + 1 )
    return false;

  for(int i = idx; i >=0 ; --i)
  {
    if(GetActivation(_activations[i])->location() != 
       cs.GetActivation(cs._saved_stack[i])->location())
      return false;
  }
  return true;
}


bool CS::isActivated(loc_t loc)
{
  vector<act_idx_t>::reverse_iterator i = _activations.rbegin();
  for(; i != _activations.rend(); ++i)
  {
    if(GetActivation(*i)->location() == loc)
      return true;
  }
  return false;
}
