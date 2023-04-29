include ../common.makefile

# store data in mysql database
USE_MYSQL=0

# keep only identified edges with uniqe source address
UNIQ_VTX=0

BIN_DIR=bin
TOOLS_DIR:=tools
CMN_OBJ_DIR=../common

TOOLS:= pin-tracer.so  profiler merge identifier pin-retracer.so
TOOLS:=$(addprefix $(BIN_DIR)/, $(TOOLS))

DB_LIBS=-pthread -lpthread -lssl -lcrypto

ifeq ($(USE_MYSQL),1)
DB_LIBS+=-lmysqlcppconn
DB_INCLUDE+=-I/usr/include/cppconn -I/usr/include/openssl
DB_FLAGS=-DUSE_MYSQL=1
endif

ifeq ($(UNIQ_VTX),1)
TRGT_FLAGS=-DUNIQ_VTX=1
endif

.PHONY: clean

all: $(TOOLS) 

$(BIN_DIR): 
	mkdir -p $@

$(TOOLS_DIR)/pin-tracer.o: $(TOOLS_DIR)/pin-tracer.cpp \
$(CMN_OBJ_DIR)/sideband.hpp $(CMN_OBJ_DIR)/image.hpp \
$(CMN_OBJ_DIR)/putils.hpp
	$(CXX) $(CXXFLAGS) $(PIN_CXXFLAGS) $(PIN_INCLUDE) -I$(CMN_OBJ_DIR) \
	-o $@ -c $< -DFORK=0

$(BIN_DIR)/pin-tracer.so: $(TOOLS_DIR)/pin-tracer.o $(CMN_OBJ_DIR)/sideband.o \
$(CMN_OBJ_DIR)/putils.o $(CMN_OBJ_DIR)/common.o | $(BIN_DIR)
	$(CXX) -shared -Wl,--hash-style=sysv $(PIN_LIBS_BEGIN) -I$(CMN_OBJ_DIR) \
	-o $@ $^ $(PIN_LINCLUDE) $(PIN_LIBS_END) 

$(TOOLS_DIR)/pin-tracer_forks2.o: $(TOOLS_DIR)/pin-tracer.cpp \
$(CMN_OBJ_DIR)/sideband.hpp $(CMN_OBJ_DIR)/image.hpp $(CMN_OBJ_DIR)/putils.hpp
	$(CXX) $(CXXFLAGS) $(PIN_CXXFLAGS) $(PIN_INCLUDE) -I$(CMN_OBJ_DIR) \
	-o $@ -c $< -DFORK=2

$(BIN_DIR)/pin-tracer_forks2.so: $(TOOLS_DIR)/pin-tracer_forks2.o \
$(CMN_OBJ_DIR)/sideband.o	$(CMN_OBJ_DIR)/putils.o $(CMN_OBJ_DIR)/common.o
	$(CXX) -shared -Wl,--hash-style=sysv $(PIN_LIBS_BEGIN)  -o $@ $^   \
		$(PIN_LINCLUDE) $(PIN_LIBS_END) 

$(TOOLS_DIR)/profiler.o: $(TOOLS_DIR)/profiler.cpp $(TOOLS_DIR)/profiler.hpp \
$(CMN_OBJ_DIR)/common.hpp $(CMN_OBJ_DIR)/image.hpp
	$(CXX)  $(CXXFLAGS) $(DB_FLAGS) $(DB_INCLUDE) -I$(CMN_OBJ_DIR) -c $<   -o $@

$(BIN_DIR)/profiler: $(TOOLS_DIR)/profiler.o $(CMN_OBJ_DIR)/common.o | $(BIN_DIR)
	$(CXX)  $(CXXFLAGS) $(DB_FLAGS) $(DB_INCLUDE) -I$(CMN_OBJ_DIR) \
	-o $@ $^ $(DB_LIBS) 

$(TOOLS_DIR)/merge.o: $(TOOLS_DIR)/merge.cpp $(CMN_OBJ_DIR)/common.hpp \
$(CMN_OBJ_DIR)/image.hpp 
	$(CXX)  $(CXXFLAGS) -I$(CMN_OBJ_DIR) -c $<  -o $@

$(BIN_DIR)/merge: $(TOOLS_DIR)/merge.o $(CMN_OBJ_DIR)/common.o | $(BIN_DIR)
	$(CXX)  $(CXXFLAGS) -I$(CMN_OBJ_DIR) -o $@ $^ 

$(TOOLS_DIR)/identifier.o: $(TOOLS_DIR)/identifier.cpp \
$(CMN_OBJ_DIR)/common.hpp $(CMN_OBJ_DIR)/image.hpp $(CMN_OBJ_DIR)/utils.hpp $(CMN_OBJ_DIR)/static.hpp 
	$(CXX)  $(CXXFLAGS) $(TRGT_FLAGS) -I$(CMN_OBJ_DIR) $(DBG) $(IDA) -c  $<  -o $@

$(BIN_DIR)/identifier: $(TOOLS_DIR)/identifier.o $(CMN_OBJ_DIR)/common.o \
$(CMN_OBJ_DIR)/static.o $(CMN_OBJ_DIR)/utils.o | $(BIN_DIR)
	$(CXX)  $(CXXFLAGS) $(TRGT_FLAGS) -I$(CMN_OBJ_DIR) -o $@ $^ 


$(BIN_DIR)/eprint: $(TOOLS_DIR)/eprint.o $(CMN_OBJ_DIR)/common.o \
$(CMN_OBJ_DIR)/utils.o | $(BIN_DIR)
	$(CXX)  $(CXXFLAGS) $(TRGT_FLAGS) -I$(CMN_OBJ_DIR) -o $@ $^ 

$(TOOLS_DIR)/eprint.o: $(TOOLS_DIR)/eprint.cpp \
$(CMN_OBJ_DIR)/common.hpp $(CMN_OBJ_DIR)/image.hpp $(CMN_OBJ_DIR)/utils.hpp 
	$(CXX)  $(CXXFLAGS) $(TRGT_FLAGS) -I$(CMN_OBJ_DIR) $(DBG) -c  $<  -o $@


$(BIN_DIR)/pin-retracer.so: $(TOOLS_DIR)/pin-retracer.o \
$(CMN_OBJ_DIR)/sideband.o $(CMN_OBJ_DIR)/putils.o \
$(CMN_OBJ_DIR)/common.o | $(BIN_DIR)
	$(CXX) -shared -Wl,--hash-style=sysv $(PIN_LIBS_BEGIN) -I$(CMN_OBJ_DIR) \
	-o $@ $^ $(PIN_LINCLUDE) $(PIN_LIBS_END) 

$(TOOLS_DIR)/pin-retracer.o: $(TOOLS_DIR)/pin-retracer.cpp \
$(CMN_OBJ_DIR)/sideband.hpp \
$(CMN_OBJ_DIR)/image.hpp $(CMN_OBJ_DIR)/putils.hpp $(CMN_OBJ_DIR)/common.hpp
	$(CXX) $(CXXFLAGS) $(PIN_CXXFLAGS) $(PIN_INCLUDE) -I$(CMN_OBJ_DIR) $(DBG) \
	-o $@ -c $< -DFORK=0

$(BIN_DIR)/pin-retracer_forks2.so: $(TOOLS_DIR)/pin-retracer_forks2.o \
$(CMN_OBJ_DIR)/sideband.o $(CMN_OBJ_DIR)/putils.o \
$(CMN_OBJ_DIR)/common.o | $(BIN_DIR)
	$(CXX) -shared -Wl,--hash-style=sysv $(PIN_LIBS_BEGIN) -I$(CMN_OBJ_DIR) \
	-o $@ $^ $(PIN_LINCLUDE) $(PIN_LIBS_END)

$(TOOLS_DIR)/pin-retracer_forks2.o: $(TOOLS_DIR)/pin-retracer.cpp \
$(CMN_OBJ_DIR)/sideband.hpp \
$(CMN_OBJ_DIR)/image.hpp $(CMN_OBJ_DIR)/common.hpp
	$(CXX) $(CXXFLAGS) $(PIN_CXXFLAGS) $(PIN_INCLUDE) -I$(CMN_OBJ_DIR) $(DBG) \
	-o $@ -c $< -DFORK=2 

# targetvtx: target.cpp
# 	$(eval TRGT_FLAGS += -DUNIQ_VTX=1)
# 	$(CXX)  $(CXXFLAGS) $(TRGT_FLAGS) -o $@ $< 	
# %.so: %.o
# 	$(CXX) -shared -Wl,--hash-style=sysv $(PIN_LIBS_BEGIN) -o $@ $< \
# 		$(PIN_LINCLUDE) $(PIN_LIBS_END)


clean:
	rm -f $(TOOLS_DIR)/*.o
	rm -fr $(BIN_DIR)
