#
# Makefile for demonstrating how to build FSDB reader test cases
#

#
# The following settings need to be modified before compiling at
# your environment:
#
#   include ./COMPILER_DEF
#   include ./FILE_LIST
#   INC_DIR = ../../../ffrAPI
#   VG_GNU_LIB = $VG_GNU_PACKAGE/lib
#   SHARD_LIB = -L../../../lib -lnffr -L../../../../access/lib -lnsys 
#
# Here is an example describing the settings at LINUX platform:
#
#   include <$NOVAS_HOME$>/FsdbReader/LINUX/COMPILER_DEF
#   include <$NOVAS_HOME$>/FsdbReader/FILE_LIST
#   INC_DIR = <$NOVAS_HOME$>/FsdbReader/
#   VG_GNU_LIB = <$VG_GNU_PACKAGE>/linux/gcc/lib64
#   SHARD_LIB = -L<$NOVAS_HOME$>/FsdbReader/LINUX -lnffr -lnsys


VERDI_VERSION ?= W-2024.09-1

arch = linux64

INC_DIR = /tools/synopsys/verdi/$(VERDI_VERSION)/share/FsdbReader
LINUX_DIR = $(INC_DIR)/$(arch)

include ./cpp/COMPILER_DEF.mk
PORT_LIB_SEARCH = $(LINUX_DIR)/libnffr.so $(LINUX_DIR)/libnsys.so

include ./cpp/FILE_LIST.mk

# VG_GNU_LIB = $(VG_GNU_PACKAGE)/linux/gcc/lib64
VG_GNU_LIB = /opt/rh/gcc-toolset-13/root/usr/lib64
SHARD_LIB = -L$(LINUX_DIR) -lnffr -lnsys

ALL_LIBS = $(SHARD_LIB) $(LOAD_LIBS)
CXXFLAGS = -I$(INC_DIR) -I$(LINUX_DIR) -I/tools/C/nayiri/conda/envs/fsdb/lib $(EXTRA_CXXFLAGS) $(MY_CXXFLAGS)

MYSUFFIXES = $(POLY_OBJ) .c .C .cpp .s .l .y .E .Fc .fmH .fm
.SUFFIXES:	$(MYSUFFIXES)

all: $(TEST_PROGRAM)

%$(EXE_EXT): ./cpp/%$(POLY_OBJ) $(INC_DIR)/ffrAPI.h
	$(CXX_LINKER) $(FSDB_TARGET) $(CXXFLAGS) $< $(PORT_LIB_SEARCH) $(ALL_LIBS)

clean:
	\rm -f $(TEST_PROGRAM) ./cpp/*$(POLY_OBJ)
