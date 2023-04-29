#!/usr/bin/python3

from  pwnlib.elf import ELF
import sys
import os
import stat

program = sys.argv[1]
feat_file = sys.argv[2]

with open(feat_file) as f:
    f.readline()
    l = f.readline()
    addr = l.split()[0]
    
feat_addr = int(addr, 16)

e=ELF(program)
addr=e.address+feat_addr
e.asm(addr,'int 3')
e.save(program+'-patched')
st = os.stat(program+'-patched')
os.chmod(program+'-patched', st.st_mode | stat.S_IEXEC)