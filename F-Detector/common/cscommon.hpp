#ifndef __CSCOMMON__
#define __CSCOMMON__

#include "image.hpp"
#include "common.hpp"

#define NAS 0x0f0f0f0f0f0f0f0f


struct ret_struct
{
  loc_t loc;
  ADDRINT val;

  bool operator==(const ret_struct& other) const
  {
    return (this->loc == other.loc && this->val == other.val);
  }
  ret_struct(loc_t l, ADDRINT v) : loc(l), val(v) {};
  ret_struct() : loc(), val(NAS) {};
};

struct syscall_struct
{
  loc_t loc;
  ADDRINT eret;
  ADDRINT vret;
  ADDRINT id;
 
  syscall_struct(loc_t l, ADDRINT i, ADDRINT v) : 
  loc(l),
  eret((ADDRINT)-1),
  vret(v), 
  id(i)
  {}

  syscall_struct(loc_t l) : 
  loc(l),
  eret((ADDRINT)-1),
  vret(NAS), 
  id(NAS)
  {}

  syscall_struct() {};
};

typedef ret_struct ret_t;
typedef syscall_struct syscall_t;
typedef pair<loc_t, size_t> act_idx_t;
typedef vector<pair<ADDRINT,ADDRINT>> mrange_t;
typedef vector<pair<ADDRINT,ADDRINT>>::iterator mrange_it;

#endif