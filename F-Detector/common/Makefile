include ../common.makefile

ELFIO_INCLUDE=-I..

TOOLS:= common.o sideband.o putils.o utils.o callstack.o static.o static-pin.o rescuepoint.o

.PHONY: clean


all: $(TOOLS)

common.o: common.cpp common.hpp image.hpp
	$(CXX)  $(CXXFLAGS) $(DBG) -c $< 

utils.o: utils.cpp
	$(CXX)  $(CXXFLAGS) $(DBG) -c $< 

rescuepoint.o: rescuepoint.cpp rescuepoint.hpp common.hpp
	$(CXX) $(CXXFLAGS) $(PIN_CXXFLAGS) $(PIN_INCLUDE) -c $<  -o $@

putils.o: putils.cpp putils.hpp image.hpp
	$(CXX)  $(CXXFLAGS)  $(PIN_CXXFLAGS) $(PIN_INCLUDE) $(DBG) -c $< -o $@

sideband.o: sideband.cpp sideband.hpp image.hpp
	$(CXX) $(CXXFLAGS) $(PIN_CXXFLAGS) $(PIN_INCLUDE) -c $<  -o $@

static.o: static.cpp static.hpp image.hpp
	$(CXX) $(ELFIO_INCLUDE) $(CXXFLAGS) $(TRGT_FLAGS) $(DBG) $(IDA) \
	-c $<  -o $@

static-pin.o: static.cpp static.hpp image.hpp
	$(CXX) $(ELFIO_INCLUDE) $(PIN_CXXFLAGS) $(PIN_INCLUDE) $(TRGT_FLAGS) $(DBG) $(IDA) \
	-DWITHPIN -c $<  -o $@

callstack.o: callstack.cpp callstack.hpp common.hpp rescuepoint.hpp static.hpp
	$(CXX) $(CXXFLAGS) $(PIN_CXXFLAGS) $(PIN_INCLUDE) $(DBG) $(IDA) -o $@ -c $<


clean:
	rm -f $(TOOLS) *.o
