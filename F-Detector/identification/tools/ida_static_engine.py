from __future__ import print_function

import idaapi
import idautils
import idc

import logging
from collections import deque
from struct import pack, unpack_from
from time import sleep
import os
import errno

# try: 
#     from miasm.core.bin_stream_ida import bin_stream_ida
#     from miasm.analysis.machine import Machine
# except:
#     pass

######################
###### COMMANDS ######
START = 0xCAFED00D   #
END   = 0xDEADBEEF   #
TYPE  = 0x01010101   #
DREFS = 0x02020202   #
CREFS = 0x03030303   #
VRET  = 0x05050505   #
FEND  = 0x06060606   #
FNAM  = 0x07070707   #
DOMIN = 0x08080808   #
######################
######################
######################
#### BRANCH TYPES ####
ICALL =  0           #
CALL  =  1           #
IJMP  =  2           #
JMP   =  3           #
RET   =  4           #
CJMP  =  5          #
NA    = -1           #
######################
######################
######################
### ISVOID RET CODE ##
FNONVOID        =  1 #
FNONVOIDWARN    =  2 #
FVOID           =  0 #
######################
######################

IDA_ERROR   = 0x0F0F0F0F
UNKOWN_CMND = 0xF0F0F0F0 
RESOLVE_NAME = 0xA0A0A0A0

COMANDS = [START, END, TYPE, DREFS, CREFS, VRET, FEND, FNAM]
COMANDS_2ARGS = [DOMIN]

IMAGE = idc.get_root_filename()


c_jumps = ["jle", "jz", "je", "ja", "jnbe", "jne", "jnz", "jnb", "jae", "jnc",
 "js", "jq", "jbe", "jna", "jnae", "jb", "jnae", "jc", "jo", "jno", "jns", "jl",
 "jnge", "jge", "jnl", "jng", "jg", "jnle", "jp", "jpe", "jnp", "jpo", "ud2",
 "jcxz", "jecxz", "jrcxz"]

u_jumps = ["jmp"]

op_type = {0 : "o_void", 1 : "o_reg", 2 : "o_mem", 3 : "o_phrase", 4 : "o_displ",
 5 : "o_imm", 6 : "o_far", 7 : "o_near"}

bbtype = {0: "fcb_normal", 1: "fcb_indjump", 2: "fcb_ret", 3: "fcb_cndret",
              4: "fcb_noret", 5: "fcb_enoret", 6: "fcb_extern", 7: "fcb_error"}

ax = [0x0, 0x10]

seen = []


def get_ida_logging_handler():
    """
    IDA logger should always be the first one (since it inits the env)
    """
    return logging.getLogger().handlers[0]


class BBL(object):
    """A basic block"""
    def __init__(self, addr):
        super(BBL, self).__init__()
        self.addr = addr
        try:
            self.func = idaapi.get_func(self.addr)
        except Exception as e:
            self.func = None
        try:
            self.fc = idaapi.FlowChart(self.func, flags=idaapi.FC_PREDS)
        except Exception as e:
            self.fc = None

        self.found = False
        self.bbl = self.get_bbl()

    def get_bbl(self):
        if self.fc:
            for bbl in self.fc:
                if bbl.start_ea == self.addr:
                    self.found = True
                    return bbl
                elif bbl.start_ea < self.addr and bbl.end_ea > self.addr :
                    return bbl
        return None

    def get_branch_type(self):
        if self.bbl == None:
            return -1

        addr = self.addr
        while addr != self.bbl.end_ea:
            mnem = idc.print_insn_mnem(addr).lower()
            if mnem.startswith("call"):
                if op_type[idc.get_operand_type(addr,0)] != "o_near" and\
                    op_type[idc.get_operand_type(addr,0)] != "o_far": 
                    return ICALL
                else:
                    return CALL
            addr = idc.next_head(addr)


        addr = idc.prev_head(self.bbl.end_ea)
        mnem = idc.print_insn_mnem(addr).lower()
        if mnem in u_jumps or mnem in c_jumps:
            if op_type[idc.get_operand_type(addr,0)] != "o_near" and\
                op_type[idc.get_operand_type(addr,0)] != "o_far": 
                return IJMP
            else:
                if mnem in u_jumps:
                    return JMP
                else:
                    return CJMP
        elif self.bbl.type >= 2 and  self.bbl.type <= 5:
            return RET

        return NA

    # we only count the data refs of type offset (read/write are too error prone)
    def get_data_refs(self):
        xrefs = list(idautils.XrefsTo(self.addr))
        return len([x for x in xrefs if (x.type == idautils.ida_xref.dr_O 
            and 'data' in idc.get_segm_name(x.frm).lower())])        


    def get_code_refs(self):
        xrefs = list(idautils.CodeRefsTo(self.addr,1))
        if len(xrefs) < 2:
            mnem = idc.print_insn_mnem(self.addr).lower() 
            if mnem == "nop" or mnem == "jmp":
                return 9999
        return len(xrefs)


    def is_nonvoid_ret(self):
        xrefs = list(idautils.CodeRefsTo(self.addr,0))
        #hueristic 1: check if any of the callers uses value in eax
        for caller in xrefs:
            if is_eax_used(idc.next_head(caller)):
                return FNONVOID
        #hueristic 2: check if function always set eax before returning
        if xrefs == [] and self.func_returns():
            return FNONVOID


        if self.func_returns():
            return FNONVOIDWARN
        return FVOID

    def func_returns(self):
        if self.fc == None:
            return False
        bbl = []
        for bb in self.fc:
            # does bbl return
            if bb.type == 2 or  bb.type == 3:
                bbl.append(bb)  
        
        if bbl == []:
            return False

        ret = True
        for bb in bbl:
            ret = ret & is_eax_set(bb)
            if ret == False:
                break
        return ret

    def get_func_return_vals(self):
        if self.fc == None:
            return []
        bbl = []
        for bb in self.fc:
            # does bbl return
            if bb.type == 2 or  bb.type == 3:
                bbl.append(bb)  
        
        if bbl == []:
            return []
        rets = []
        for bb in bbl:
            vals = ret_vals(bb)
            if vals != []:
                rets += vals
        return rets

    def func_bound(self):
        if self.func == None:
            return (IDA_ERROR, IDA_ERROR)
        fend = idc.prev_head(self.func.end_ea)
        if self.addr == fend:
            if idc.print_insn_mnem(self.addr) in u_jumps:
                return (RESOLVE_NAME, RESOLVE_NAME)
            else:
                return (IDA_ERROR, IDA_ERROR)
        return (self.func.start_ea, fend)

    def func_name(self):
        name = get_func_name(self.addr)
        if name[0] == '.':
            name = name[1:]
        if len(name) >=50:
            name = name[:49]
        return name

    def is_dominant(self, RP):
        seen = []
        if list(idautils.CodeRefsTo(self.func.start_ea,0)) == []:
            return True
        if dominates(self.func.start_ea, RP) == True:
            return 1
        else:
            return 0


def dominates(feat, RP):
    xrefs = list(idautils.CodeRefsTo(feat,0))
    if xrefs == []:
        return False
    ret = True
    for x in xrefs:
        caller = idaapi.get_func(x).start_ea
        if caller in seen:
            continue
        elif caller == RP:
            return True
        else:
            ret &= dominates(caller, RP)
            if ret == False:
                return ret
        seen.append(caller)
    return ret        
    

     
def ret_vals(bbl):
    iaddr = idc.prev_head(bbl.end_ea)
    while idc.next_head(iaddr) != bbl.start_ea:
        if is_eax_written(iaddr) and is_nothing_read(iaddr):
            return [idc.get_operand_value(iaddr,1)]
        if is_eax_read(iaddr):
            return []
        iaddr = idc.prev_head(iaddr)

    ret = []
    for bb in bbl.preds():
        ret = ret + ret_vals(bb)
        
    return ret

def is_eax_set(bbl):
    iaddr = idc.prev_head(bbl.end_ea)
    iaddr = idc.prev_head(bbl.end_ea)
    while idc.next_head(iaddr) != bbl.start_ea:
        if is_eax_written(iaddr):
            return True
        if is_eax_read(iaddr):
            return False
        iaddr = idc.prev_head(iaddr)

    ret = True
    for bb in bbl.preds():
        ret = ret & is_eax_set(bb)
        if ret == False:
            break
    return ret



def is_eax_written(inst):
    read, write, ira = get_rw(inst)
    for r in write:
        if r in ira.arch.regs.r_eax_all.str:
            return True
    return False

    # mnem = idc.print_insn_mnem(inst)
    # if mnem.startswith("call"):
    #     return True
    # elif mnem.startswith("ret"):
    #     return True
    # elif mnem.startswith("leave"):
    #     return True
    # else:
    #     if op_type[idc.get_operand_type(inst,0)] != 'o_reg':
    #         return False
    #     reg = idc.get_operand_value(inst,0)
    #     if reg in ax:
    #         return True
    #     return False

def is_eax_read(inst):
    read, write, ira = get_rw(inst)
    for r in read:
        if r in ira.arch.regs.r_eax_all.str:
            return True
    return False


def is_nothing_read(inst):
    read, write, ira = get_rw(inst)
    if read == set():
        return True
    else:
        return False
        
def is_eax_used(addr):
    mnem = idc.print_insn_mnem(addr)
    
    # logger.info("IS_EAX_USED: ", idc.generate_disasm_line(addr, 0))
    if mnem in u_jumps:
        # logger.info("unconditional jump")
        return is_eax_used(idc.get_operand_value(addr, 0))

    elif mnem in c_jumps:
        # logger.info("conditional jump")
        return is_eax_used(idc.get_operand_value(addr, 0))\
        or is_eax_used(idc.next_head(addr))

    else:
        if(is_eax_read(addr)):
            logger.info("eax is read")
            return True

        if(is_eax_written(addr)):
            logger.info("eax is written")
            return False

        return is_eax_used(idc.next_head(addr))

def get_rw(start):
    bs = bin_stream_ida()
    machine = guess_machine()
    mdis = machine.dis_engine(bs) 

    mdis.dont_dis = [idc.next_head(start)]
    asmcfg = mdis.dis_multiblock(start)
    ira = machine.ira(loc_db=mdis.loc_db)
    ircfg = ira.new_ircfg_from_asmcfg(asmcfg)

    read = set()
    write = set()

    for lbl, irblock in ircfg.blocks.items():
        for assignblk in irblock:
            rw = assignblk.get_rw()
            for dst, reads in rw.items():
                read = read.union(set([str(x) for x in reads]))
                write.add(str(dst))

    return read, write, ira


def guess_machine(addr=None):
    "Return an instance of Machine corresponding to the IDA guessed processor"

    processor_name = idc.get_inf_attr(INF_PROCNAME)
    info = idaapi.get_inf_structure()

    if info.is_64bit():
        size = 64
    elif info.is_32bit():
        size = 32
    else:
        size = None

    if processor_name == "metapc":
        size2machine = {
            64: "x86_64",
            32: "x86_32",
            None: "x86_16",
        }

        machine = Machine(size2machine[size])

    elif processor_name == "ARM":
        # TODO ARM/thumb
        # hack for thumb: set armt = True in globals :/
        # set bigendiant = True is bigendian
        # Thumb, size, endian
        info2machine = {(True, 32, True): "armtb",
                        (True, 32, False): "armtl",
                        (False, 32, True): "armb",
                        (False, 32, False): "arml",
                        (False, 64, True): "aarch64b",
                        (False, 64, False): "aarch64l",
                        }

        # Get T reg to detect arm/thumb function
        # Default is arm
        is_armt = False
        if addr is not None:
            t_reg = GetReg(addr, "T")
            is_armt = t_reg == 1

        is_bigendian = info.is_be()
        infos = (is_armt, size, is_bigendian)
        if not infos in info2machine:
            raise NotImplementedError('not fully functional')
        machine = Machine(info2machine[infos])

        from miasm.analysis.disasm_cb import guess_funcs, guess_multi_cb
        from miasm.analysis.disasm_cb import arm_guess_subcall, arm_guess_jump_table
        guess_funcs.append(arm_guess_subcall)
        guess_funcs.append(arm_guess_jump_table)

    elif processor_name == "msp430":
        machine = Machine("msp430")
    elif processor_name == "mipsl":
        machine = Machine("mips32l")
    elif processor_name == "mipsb":
        machine = Machine("mips32b")
    elif processor_name == "PPC":
        machine = Machine("ppc32b")
    else:
        print(repr(processor_name))
        raise NotImplementedError('not fully functional')

    return machine


# class Edge(object):
#     """An edge between two bbls """
#     def __init__(self, src, dst):
#         super(Edge, self).__init__()
#         self.src = BBL(src)
#         self.dst = BBL(dst)
#         self.type = self.src.get_branch_type()

#     def check_destination_uniqueness(self):
#         if self.dst.found == False:
#             return 1
#         else:
#             if self.type == IJMP:
#                 xrefs = list(idautils.CodeRefsTo(self.dst.addr,1))
#                 if len(xrefs) == 0:
#                     self.type = ICALL
#                 else:
#                     self.type = JMP

#             if self.type == CALL or self.type == JMP:
#                 xrefs = list(idautils.CodeRefsTo(self.dst.addr,1))
#                 if len(xrefs) > 1:
#                     return 0
#                 elif len(xrefs) == 0:
#                     return 1
#                 else:
#                     xref = BBL(xrefs[0])
#                     if xref.bbl != None and xref.bbl.start_ea != self.src.addr:
#                         return 0
#                     else:
#                         return 1

#             if self.type == ICALL:
#                 if self.dst.get_number_of_pointers() > 1:
#                     return 0
#                 else:
#                     return 1

#             if self.type == RET or self.type == NA:
#                 return 1 



def recv_from_pipe():
    fifo = "/tmp/cmnd_" + IMAGE
    if not os.path.exists(fifo):
        logger.info("ERROR...")
        return IDA_ERROR

    with open(fifo,'rb') as pipe:
        d = pipe.read()
        command = unpack_from('I', d, 0)[0]
        # logger.info("command: "  + hex(command))
        
        if command == START or command == END:
            return (command, )
        elif command in COMANDS:
            addr = unpack_from('Q', d, 4)[0]
            return (command, addr)
        elif command in COMANDS_2ARGS:
            feat = unpack_from('Q', d, 4)[0]
            RP = unpack_from('Q', d, 8)[0]
            return (command, feat, RP)
        else:
            return UNKOWN_CMND

    logger.info("ERROR END...")
    return IDA_ERROR


def send_in_pipe(cmd, response = None):
    fifo = "/tmp/resp_" + IMAGE
    
    if not os.path.exists(fifo):
        return IDA_ERROR

    with open(fifo,'wb') as pipe:
        if cmd == START or cmd == END or cmd == IDA_ERROR:
            res = pack('I', cmd)
        elif cmd == FEND:
            res = pack('2Q', response[0], response[1])
        elif cmd == FNAM:
            res = pack('49s', bytes(response,"utf-8"))
        else:
            res = pack('i', response)
        pipe.write(res)

    return True

def clean_the_mess():
    ## moved to the c code ##
    # idb = "/tmp/" + IMAGE + ".i64"

    # if os.path.exists(idb):
    #     os.remove(idb)

    print("FINISHED ANALYSING BYE BYE...")
    return True

def main():
    while True:
        cmnd = recv_from_pipe()
        if cmnd == IDA_ERROR:
            break

        elif cmnd == UNKOWN_CMND:
            send_in_pipe(END)
            break

        elif type(cmnd) == tuple:
            command = cmnd[0]   
            if len(cmnd) == 1:
                if command == START:
                    logger.info("Received START...")
                    if send_in_pipe(START) == IDA_ERROR:
                        break

                    idc.auto_wait()
                    continue

                elif command == END:  
                    logger.info("Received END...")
                    if not clean_the_mess():
                        break
                    if send_in_pipe(END) == IDA_ERROR:
                        break
                    idc.qexit(0)
                    return 
            elif len(cmnd) >= 2:
                bbl = BBL(cmnd[1])
                if command == TYPE:
                    logger.info("Received TYPE...")
                    res = bbl.get_branch_type()
                elif command == DREFS:
                    logger.info("Received DREFS...")
                    res = bbl.get_data_refs()
                elif command == CREFS:
                    logger.info("Received CREFS...")    
                    res = bbl.get_code_refs()
                elif command == VRET:
                    logger.info("Received VRET...")    
                    res = bbl.is_nonvoid_ret()
                elif command == FEND:
                    logger.info("Received FEND...")    
                    res = bbl.func_bound()
                elif command == FNAM:
                    logger.info("Received FNAM...")    
                    res = bbl.func_name()
                elif command == DOMIN:
                    logger.info("Received DOMIN...")    
                    res = bbl.is_dominant(cmnd[2])
                if send_in_pipe(command, res) == IDA_ERROR:
                    break
        else:
            break

    idc.qexit(-1)
    return

if __name__ == "__main__":
    logging.basicConfig(level=logging.DEBUG)
    get_ida_logging_handler().setLevel(logging.INFO)
    logger = logging.getLogger("SE:")
    logger.info("Started...")

    main()
