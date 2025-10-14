NAMES = print_header \
        dump_embedding \

PROGRAM_NAMES := $(addsuffix .cpp, $(NAMES))
TEST_PROGRAM := $(addsuffix $(EXE_EXT), $(NAMES))
