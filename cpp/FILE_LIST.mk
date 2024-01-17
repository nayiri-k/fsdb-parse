NAMES = dump_signal \
        dump_toggles \
        dump_ones \
        get_sim_endtime \
        print_header \

PROGRAM_NAMES := $(addsuffix .cpp, $(NAMES))
TEST_PROGRAM := $(addsuffix $(EXE_EXT), $(NAMES))
