GCC_BIN = /opt/rh/devtoolset-8/root/usr/bin/gcc
GXX_BIN = /opt/rh/devtoolset-8/root/usr/bin/g++
# GCC_BIN = /usr/bin/gcc
# GXX_BIN = /usr/bin/g++
CC = $(GCC_BIN)
CC_LINKER = $(GCC_BIN)
CXX = $(GXX_BIN)
CXX_LINKER = $(GXX_BIN)
LOAD_LIBS = -lXft -lXt -lXmu -lX11 -lXext -lfontconfig -lm -lnsl -liberty -lpthread -lrt -ldl -Wl,-E
# omitted: -lXss 
FSDB_LINKER_OUT = -o
FSDB_TARGET = -o $@
EXTRA_CXXFLAGS = -DPPORT_LINUX -DSVR4 -D_GLIBCXX_USE_CXX11_ABI=0 -DFSDB_USE_32B_IDCODE -DSVR4 -DNOVAS_OEM -D__NO_CTYPE -D_REENTRANT -DFSDB_LIBRARY
POLY_OBJ = .o
POLY_LIB = .a
EXE_EXT =
PORT_LIB_SEARCH =
