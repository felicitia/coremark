# Copyright 2018 Embedded Microprocessor Benchmark Consortium (EEMBC)
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#    http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
# 
# Original Author: Shay Gal-on

# Make sure the default target is to simply build and run the benchmark.
RSTAMP = v1.0

.PHONY: run score
run: $(OUTFILE) rerun score

score:
	@echo "Check run1.log and run2.log for results."
	@echo "See README.md for run and reporting rules." 
	
ifndef PORT_DIR
# Ports for a couple of common self hosted platforms
UNAME=$(shell if command -v uname 2> /dev/null; then uname ; fi)
ifneq (,$(findstring CYGWIN,$(UNAME)))
PORT_DIR=cygwin
endif
ifneq (,$(findstring Darwin,$(UNAME)))
PORT_DIR=macos
endif
ifneq (,$(findstring FreeBSD,$(UNAME)))
PORT_DIR=freebsd
endif
ifneq (,$(findstring Linux,$(UNAME)))
PORT_DIR=linux
endif
endif
ifndef PORT_DIR
$(error PLEASE define PORT_DIR! (e.g. make PORT_DIR=simple)) 
endif
vpath %.c $(PORT_DIR)
vpath %.h $(PORT_DIR)
vpath %.mak $(PORT_DIR)
include $(PORT_DIR)/core_portme.mak

ifndef ITERATIONS
ITERATIONS=0
endif
ifdef REBUILD
FORCE_REBUILD=force_rebuild
endif

# Define paths to LLVM build and tools
LLVM_BUILD_DIR = /home/yixue/isi-llvm/build
OPT = $(LLVM_BUILD_DIR)/bin/opt
CLANG = $(LLVM_BUILD_DIR)/bin/clang
LLC = $(LLVM_BUILD_DIR)/bin/llc
MODE = 3
CFLAGS += -DITERATIONS=$(ITERATIONS)

CORE_FILES = core_list_join core_main core_matrix core_state core_util
ORIG_SRCS = $(addsuffix .c,$(CORE_FILES))
SRCS = $(ORIG_SRCS) $(PORT_SRCS)
OBJS = $(addprefix $(OPATH),$(addsuffix $(OEXT),$(CORE_FILES)) $(PORT_OBJS))
OUTNAME = coremark$(EXE)
OUTFILE = $(OPATH)$(OUTNAME)
LOUTCMD = $(OFLAG) $(OUTFILE) $(LFLAGS_END)
OUTCMD = $(OUTFLAG) $(OUTFILE) $(LFLAGS_END)
# OUTCMD = -o $(OUTFILE)

HEADERS = coremark.h ftlib.h
CHECK_FILES = $(ORIG_SRCS) $(HEADERS)

$(OPATH):
	$(MKDIR) $(OPATH)

.PHONY: compile link
ifdef SEPARATE_COMPILE
$(OPATH)$(PORT_DIR):
	$(MKDIR) $(OPATH)$(PORT_DIR)

# Rule to generate LLVM IR from source
# compile: $(OPATH) $(SRCS) $(HEADERS)
# 	$(CLANG) -emit-llvm -S $(CFLAGS) $(XCFLAGS) $(SRCS)

# Apply HPSC-CFA LLVM pass
# transform: compile
# 	@echo "Applying LLVM pass..."
# 	$(foreach file,$(addsuffix .ll,$(CORE_FILES)), \
# 		$(OPT) -S -mode=$(MODE) -passes=hpsc-cfa $(file) -o $(basename $(file))_transformed.ll;)

# Rule to compile LLVM IR into executable
# link: transform
# 	$(CLANG) $(addsuffix .ll,$(CORE_FILES)) $(LFLAGS) $(LOUTCMD)

# compile: $(OPATH) $(OPATH)$(PORT_DIR) $(OBJS) $(HEADERS) 
# link: compile 
# 	$(LD) $(LFLAGS) $(XLFLAGS) $(OBJS) $(LOUTCMD)
	
else

# Rule to generate LLVM IR from source
compile: $(OPATH) $(SRCS) $(HEADERS)
	$(CLANG) -emit-llvm -S $(CFLAGS) $(XCFLAGS) $(SRCS)

# Apply HPSC-CFA LLVM pass
transform: compile
	@echo "Applying CFA pass..."
	$(foreach file,$(addsuffix .ll,$(CORE_FILES)), \
		$(OPT) -S -mode=$(MODE) -passes=hpsc-cfa $(file) -o $(basename $(file))_transformed.ll;)

# Rule to compile LLVM IR into executable
link: transform
	$(CLANG) $(addsuffix _transformed.ll,$(CORE_FILES)) core_portme.ll -o $(OUTFILE) -lrt
endif

$(OUTFILE): $(SRCS) $(HEADERS) Makefile core_portme.mak $(EXTRA_DEPENDS) $(FORCE_REBUILD)
	$(MAKE) port_prebuild
	$(MAKE) link
	$(MAKE) port_postbuild

.PHONY: testNMR
testNMR: $(OPATH) $(SRCS) $(HEADERS) ftlib.c
	$(CLANG) -fPIE -emit-llvm -c $(CFLAGS) $(XCFLAGS) $(SRCS)
	@echo "Applying NMR pass..."
	$(foreach file,$(addsuffix .bc,$(CORE_FILES)), \
		$(OPT) -passes=ft -ft-auto-optimization-level=0 -o $(basename $(file)).bc0 < $(file);)
	$(foreach file,$(addsuffix .bc0,$(CORE_FILES)), \
		$(LLC) -relocation-model=pic -filetype=obj -o $(basename $(file)).o $(basename $(file)).bc0;)
	$(LLC) -relocation-model=pic -filetype=obj -o core_portme.o core_portme.bc
	$(CLANG) -c -fPIE -o ftlib.o ftlib.c
	$(CLANG) -o coremarkNMR.exe ftlib.o core_portme.o $(addsuffix .o,$(CORE_FILES)) -lrt


.PHONY: rerun
rerun: 
	$(MAKE) XCFLAGS="$(XCFLAGS) -DPERFORMANCE_RUN=1" load run1.log
	$(MAKE) XCFLAGS="$(XCFLAGS) -DVALIDATION_RUN=1" load run2.log

PARAM1=$(PORT_PARAMS) 0x0 0x0 0x66 $(ITERATIONS)
PARAM2=$(PORT_PARAMS) 0x3415 0x3415 0x66 $(ITERATIONS)
PARAM3=$(PORT_PARAMS) 8 8 8 $(ITERATIONS)

run1.log-PARAM=$(PARAM1) 7 1 2000
run2.log-PARAM=$(PARAM2) 7 1 2000 
run3.log-PARAM=$(PARAM3) 7 1 1200

run1.log run2.log run3.log: load
	$(MAKE) port_prerun
	$(RUN) $(OUTFILE) $($(@)-PARAM) > $(OPATH)$@
	$(MAKE) port_postrun
	
.PHONY: gen_pgo_data
gen_pgo_data: run3.log

.PHONY: load
load: $(OUTFILE)
	$(MAKE) port_preload
	$(LOAD) $(OUTFILE)
	$(MAKE) port_postload

.PHONY: clean
clean:
	rm -f $(OUTFILE) $(OBJS) $(OPATH)*.log *.info $(OPATH)index.html $(PORT_CLEAN)

.PHONY: force_rebuild
force_rebuild:
	echo "Forcing Rebuild"
	
.PHONY: check
check:
	md5sum -c coremark.md5 

ifdef ETC
# Targets related to testing and releasing CoreMark. Not part of the general release!
include Makefile.internal
endif	
