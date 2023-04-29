
#  ifeq ($(PIN_HOME),)
#  $(error PIN_HOME variable need to be set, before building)
#  endif
IDA:=-DIDA="$(IDA_T_EXE)"

ifeq ($(DEBUG),1)
DBG:=-DDEBUG
ifeq ($(DEBUG_IDA),1)
DBG+=-DDEBUG_IDA 
IDA:=-DIDA="$(IDA_EXE)"
endif
endif

ifeq ($(MAKECMDGOALS),x86)
TARGET_x86=1
endif

ifdef TARGET_x86
TARGET_ARCH=-DTARGET_IA32
TARGET_HOST=-DHOST_IA32
CRT_ARCH_I=arch-x86
RT_DIR=ia32
CXX=g++ -m32
else
TARGET_ARCH=-DTARGET_IA32E
TARGET_HOST=-DHOST_IA32E
CRT_ARCH_I=arch-x86_64
RT_DIR=intel64
CXX=g++ -m64
endif



PIN_CXXFLAGS=-DBIGARRAY_MULTIPLIER=1 -Wall -Wno-unknown-pragmas \
	-D__PIN__=1 -DPIN_CRT=1 -fno-stack-protector -fno-exceptions \
	-funwind-tables -fasynchronous-unwind-tables -fno-rtti \
	$(TARGET_ARCH) $(TARGET_HOST) -fPIC -DTARGET_LINUX -fabi-version=2 \
	-fomit-frame-pointer -fno-strict-aliasing

PIN_INCLUDE=-I$(PIN_HOME)/source/include/pin \
	-I$(PIN_HOME)/source/include/pin/gen \
	-isystem $(PIN_HOME)/extras/stlport/include \
	-isystem $(PIN_HOME)/extras/libstdc++/include \
	-isystem $(PIN_HOME)/extras/crt/include \
	-isystem $(PIN_HOME)/extras/crt/include/$(CRT_ARCH_I) \
	-isystem $(PIN_HOME)/extras/crt/include/kernel/uapi \
	-isystem $(PIN_HOME)/extras/crt/include/kernel/uapi/asm-x86 \
	-I$(PIN_HOME)/extras/components/include \
	-I$(PIN_HOME)/extras/xed-$(RT_DIR)/include/xed \
	-I$(PIN_HOME)/source/tools/InstLib

PIN_LIBS_BEGIN=$(PIN_HOME)/$(RT_DIR)/runtime/pincrt/crtbeginS.o -Wl,-Bsymbolic \
	-Wl,--version-script=$(PIN_HOME)/source/include/pin/pintool.ver \
	-fabi-version=2    

PIN_LINCLUDE=-L$(PIN_HOME)/$(RT_DIR)/runtime/pincrt -L$(PIN_HOME)/$(RT_DIR)/lib \
	-L$(PIN_HOME)/$(RT_DIR)/lib-ext -L$(PIN_HOME)/extras/xed-$(RT_DIR)/lib 

PIN_LIBS_END=-lpin -lxed $(PIN_HOME)/$(RT_DIR)/runtime/pincrt/crtendS.o \
	-lpin3dwarf  -ldl-dynamic -nostdlib -lstlport-dynamic -lm-dynamic \
	-lc-dynamic -lunwind-dynamic

CXXFLAGS=-Wall -Werror -g -O3  -D_GLIBCXX_USE_CXX11_ABI=0
LDFLAGS=

CFLAGS=-Wall -O3 -Werror

