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
# 
# In run time:
# setenv LD_LIBRARY_PATH $NOVAS_HOME$/FsdbReader/LINUX:$VG_GNU_PACKAGE/linux/gcc/lib:$LD_LIBRARY_PATH

arch = linux64

INC_DIR = /tools/synopsys/verdi/P-2019.06-SP2-12/share/FsdbReader
LINUX_DIR = $(INC_DIR)/$(arch)
# INC_DIR = /tools/C/nayiri/power/power-analysis/fsdb


include ./cpp/COMPILER_DEF.mk
PORT_LIB_SEARCH = $(LINUX_DIR)/libnffr.so $(LINUX_DIR)/libnsys.so

include ./cpp/FILE_LIST.mk

# VG_GNU_LIB = $(VG_GNU_PACKAGE)/linux/gcc/lib64
VG_GNU_LIB = /opt/rh/devtoolset-8/root/usr/lib64
SHARD_LIB = -L$(LINUX_DIR) -lnffr -lnsys

# in run time:
# export LD_LIBRARY_PATH=/tools/synopsys/verdi/P-2019.06-SP2-12/share/FsdbReader/linux64:/opt/rh/devtoolset-8/root/usr/lib64:${LD_LIBRARY_PATH}

ALL_LIBS = $(SHARD_LIB) $(LOAD_LIBS)
CXXFLAGS = -I$(INC_DIR) -I$(LINUX_DIR) -I/tools/C/nayiri/conda/envs/fsdb/lib $(EXTRA_CXXFLAGS) $(MY_CXXFLAGS)

MYSUFFIXES = $(POLY_OBJ) .c .C .cpp .s .l .y .E .Fc .fmH .fm
.SUFFIXES:	$(MYSUFFIXES)

all: $(TEST_PROGRAM)

dump_signal$(EXE_EXT): ./cpp/dump_signal$(POLY_OBJ) $(INC_DIR)/ffrAPI.h 
	$(CXX_LINKER) $(FSDB_TARGET) $(CXXFLAGS) ./cpp/dump_signal$(POLY_OBJ) $(PORT_LIB_SEARCH) $(ALL_LIBS)

ds$(EXE_EXT): ./cpp/dump_signals$(POLY_OBJ) $(INC_DIR)/ffrAPI.h 
	$(CXX_LINKER) $(FSDB_TARGET) $(CXXFLAGS) ./cpp/dump_signals$(POLY_OBJ) $(PORT_LIB_SEARCH) $(ALL_LIBS)

db$(EXE_EXT): ./cpp/dump_sigbits$(POLY_OBJ) $(INC_DIR)/ffrAPI.h 
	$(CXX_LINKER) $(FSDB_TARGET) $(CXXFLAGS) ./cpp/dump_sigbits$(POLY_OBJ) $(PORT_LIB_SEARCH) $(ALL_LIBS)

dt$(EXE_EXT): ./cpp/dump_toggles$(POLY_OBJ) $(INC_DIR)/ffrAPI.h 
	$(CXX_LINKER) $(FSDB_TARGET) $(CXXFLAGS) ./cpp/dump_toggles$(POLY_OBJ) $(PORT_LIB_SEARCH) $(ALL_LIBS)

do$(EXE_EXT): ./cpp/dump_ones$(POLY_OBJ) $(INC_DIR)/ffrAPI.h 
	$(CXX_LINKER) $(FSDB_TARGET) $(CXXFLAGS) ./cpp/dump_ones$(POLY_OBJ) $(PORT_LIB_SEARCH) $(ALL_LIBS)

et$(EXE_EXT): ./cpp/get_sim_endtime$(POLY_OBJ) $(INC_DIR)/ffrAPI.h 
	$(CXX_LINKER) $(FSDB_TARGET) $(CXXFLAGS) ./cpp/get_sim_endtime$(POLY_OBJ) $(PORT_LIB_SEARCH) $(ALL_LIBS)

ph$(EXE_EXT): ./cpp/print_header$(POLY_OBJ) $(INC_DIR)/ffrAPI.h 
	$(CXX_LINKER) $(FSDB_TARGET) $(CXXFLAGS) ./cpp/print_header$(POLY_OBJ) $(PORT_LIB_SEARCH) $(ALL_LIBS)

pi$(EXE_EXT): ./cpp/print_info$(POLY_OBJ) $(INC_DIR)/ffrAPI.h 
	$(CXX_LINKER) $(FSDB_TARGET) $(CXXFLAGS) ./cpp/print_info$(POLY_OBJ) $(PORT_LIB_SEARCH) $(ALL_LIBS)

argp$(EXE_EXT): ./cpp/argp$(POLY_OBJ) $(INC_DIR)/ffrAPI.h 
	$(CXX_LINKER) $(FSDB_TARGET) $(CXXFLAGS) ./cpp/argp$(POLY_OBJ) $(PORT_LIB_SEARCH) $(ALL_LIBS)


rv$(EXE_EXT): ./cpp/read_verilog$(POLY_OBJ) $(INC_DIR)/ffrAPI.h 
	$(CXX_LINKER) $(FSDB_TARGET) $(CXXFLAGS) ./cpp/read_verilog$(POLY_OBJ) $(PORT_LIB_SEARCH) $(ALL_LIBS)

rvnk$(EXE_EXT): ./cpp/read_verilog_nk$(POLY_OBJ) $(INC_DIR)/ffrAPI.h 
	$(CXX_LINKER) $(FSDB_TARGET) $(CXXFLAGS) ./cpp/read_verilog_nk$(POLY_OBJ) $(PORT_LIB_SEARCH) $(ALL_LIBS)


.cpp$(POLY_OBJ): $(PROGRAM_NAMES)
	echo $*.cpp '->' $*$(POLY_OBJ)
	echo "$(CXX) $(CXXFLAGS) -c $*.cpp"
	$(CXX) $(CXXFLAGS) -c $*.cpp -o $*$(POLY_OBJ)

clean:
	\rm -f $(TEST_PROGRAM) ./cpp/*$(POLY_OBJ)

# rv$(EXE_EXT): read_verilog$(POLY_OBJ) $(INC_DIR)/ffrAPI.h 
# 	$(CXX_LINKER) $(FSDB_TARGET) $(CXXFLAGS) read_verilog$(POLY_OBJ) $(PORT_LIB_SEARCH) $(ALL_LIBS)

# non_shared_ffrobj$(EXE_EXT): non_shared_ffrobj$(POLY_OBJ) $(INC_DIR)/ffrAPI.h 
# 	$(CXX_LINKER) $(FSDB_TARGET) $(CXXFLAGS) non_shared_ffrobj$(POLY_OBJ) $(PORT_LIB_SEARCH) $(ALL_LIBS)

# read_nanosim$(EXE_EXT): read_nanosim$(POLY_OBJ) $(INC_DIR)/ffrAPI.h 
# 	$(CXX_LINKER) $(FSDB_TARGET) $(CXXFLAGS) read_nanosim$(POLY_OBJ) $(PORT_LIB_SEARCH) $(ALL_LIBS)

# jt$(EXE_EXT): jump_test$(POLY_OBJ) $(INC_DIR)/ffrAPI.h 
# 	$(CXX_LINKER) $(FSDB_TARGET) $(CXXFLAGS) jump_test$(POLY_OBJ) $(PORT_LIB_SEARCH) $(ALL_LIBS)

# read_vhdl$(EXE_EXT): read_vhdl$(POLY_OBJ) $(INC_DIR)/ffrAPI.h 
# 	$(CXX_LINKER) $(FSDB_TARGET) $(CXXFLAGS) read_vhdl$(POLY_OBJ) $(PORT_LIB_SEARCH) $(ALL_LIBS)

# read_mix$(EXE_EXT): read_mix$(POLY_OBJ) $(INC_DIR)/ffrAPI.h 
# 	$(CXX_LINKER) $(FSDB_TARGET) $(CXXFLAGS) read_mix$(POLY_OBJ) $(PORT_LIB_SEARCH) $(ALL_LIBS)

# read_analog$(EXE_EXT): read_analog$(POLY_OBJ) $(INC_DIR)/ffrAPI.h 
# 	$(CXX_LINKER) $(FSDB_TARGET) $(CXXFLAGS) read_analog$(POLY_OBJ) $(PORT_LIB_SEARCH) $(ALL_LIBS)

# read_offset$(EXE_EXT): read_offset$(POLY_OBJ) $(INC_DIR)/ffrAPI.h 
# 	$(CXX_LINKER) $(FSDB_TARGET) $(CXXFLAGS) read_offset$(POLY_OBJ) $(PORT_LIB_SEARCH) $(ALL_LIBS)

# all_vcs_have_been_read$(EXE_EXT): all_vcs_have_been_read$(POLY_OBJ) $(INC_DIR)/ffrAPI.h 
# 	$(CXX_LINKER) $(FSDB_TARGET) $(CXXFLAGS) all_vcs_have_been_read$(POLY_OBJ) $(PORT_LIB_SEARCH) $(ALL_LIBS)

# test_sig_arr$(EXE_EXT): test_sig_arr$(POLY_OBJ) $(INC_DIR)/ffrAPI.h 
# 	$(CXX_LINKER) $(FSDB_TARGET) $(CXXFLAGS) test_sig_arr$(POLY_OBJ) $(PORT_LIB_SEARCH) $(ALL_LIBS)

# free_api$(EXE_EXT): free_api$(POLY_OBJ) $(INC_DIR)/ffrAPI.h 
# 	$(CXX_LINKER) $(FSDB_TARGET) $(CXXFLAGS) free_api$(POLY_OBJ) $(PORT_LIB_SEARCH) $(ALL_LIBS)

# trvs_mda_cell$(EXE_EXT): trvs_mda_cell$(POLY_OBJ) $(INC_DIR)/ffrAPI.h 
# 	$(CXX_LINKER) $(FSDB_TARGET) $(CXXFLAGS) trvs_mda_cell$(POLY_OBJ) $(PORT_LIB_SEARCH) $(ALL_LIBS)

# update_incore_vc_api$(EXE_EXT): update_incore_vc_api$(POLY_OBJ) $(INC_DIR)/ffrAPI.h 
# 	$(CXX_LINKER) $(FSDB_TARGET) $(CXXFLAGS) update_incore_vc_api$(POLY_OBJ) $(PORT_LIB_SEARCH) $(ALL_LIBS)

# print_compiler_version$(EXE_EXT): print_compiler_version$(POLY_OBJ) $(INC_DIR)/ffrAPI.h 
# 	$(CXX_LINKER) $(FSDB_TARGET) $(CXXFLAGS) print_compiler_version$(POLY_OBJ) $(PORT_LIB_SEARCH) $(ALL_LIBS)

# time_based$(EXE_EXT): time_based$(POLY_OBJ) $(INC_DIR)/ffrAPI.h 
# 	$(CXX_LINKER) $(FSDB_TARGET) $(CXXFLAGS) time_based$(POLY_OBJ) $(PORT_LIB_SEARCH) $(ALL_LIBS)

# test_file_info$(EXE_EXT): test_file_info$(POLY_OBJ) $(INC_DIR)/ffrAPI.h 
# 	$(CXX_LINKER) $(FSDB_TARGET) $(CXXFLAGS) test_file_info$(POLY_OBJ) $(PORT_LIB_SEARCH) $(ALL_LIBS)

# transaction$(EXE_EXT): transaction$(POLY_OBJ) $(INC_DIR)/ffrAPI.h 
# 	$(CXX_LINKER) $(FSDB_TARGET) $(CXXFLAGS) transaction$(POLY_OBJ) $(PORT_LIB_SEARCH) $(ALL_LIBS)

