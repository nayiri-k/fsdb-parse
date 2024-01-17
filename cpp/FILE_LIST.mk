PROGRAM_NAMES = read_verilog.cpp jump_test.cpp read_vhdl.cpp \
                all_vcs_have_been_read.cpp free_api.cpp read_analog.cpp \
                read_mix.cpp read_offset.cpp test_file_info.cpp \
                test_sig_arr.cpp time_based.cpp transaction.cpp \
                trvs_mda_cell.cpp non_shared_ffrobj.cpp

TEST_PROGRAM =  rv$(EXE_EXT) \
                jt$(EXE_EXT) \
                read_vhdl$(EXE_EXT) \
                all_vcs_have_been_read$(EXE_EXT) \
                free_api$(EXE_EXT) \
                read_analog$(EXE_EXT) \
                read_mix$(EXE_EXT) \
                read_offset$(EXE_EXT) \
                test_file_info$(EXE_EXT) \
                test_sig_arr$(EXE_EXT) \
                time_based$(EXE_EXT) \
                transaction$(EXE_EXT) \
                trvs_mda_cell$(EXE_EXT) \
                non_shared_ffrobj$(EXE_EXT)

