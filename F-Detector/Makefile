include configure.makefile 

MAKE=make all
CLEAN=make clean
TARGETS:=common identification

export
.PHONY: $(TARGETS) clean

all: $(TARGETS)

common:
	$(MAKE) -C $@

identification:
	$(MAKE) -C $@

clean:
	for dir in $(TARGETS) ; do \
	$(CLEAN) -C $$dir ; \
	done
