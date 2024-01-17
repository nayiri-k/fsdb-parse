/* *****************************************************************************
// [read_verilog.cpp]
//
//  Copyright 1999-2009 SPRINGSOFT. All Rights Reserved.
//
// Except as specified in the license terms of SPRINGSOFT, this material may not be copied, modified,
// re-published, uploaded, executed, or distributed in any way, in any medium,
// in whole or in part, without prior written permission from SPRINGSOFT.
// ****************************************************************************/
//
// Program Name	: read_verilog.cpp
//
// Purpose	: Demonstrate how to call fsdb reader APIs to access 
//		  the value changes of verilog type fsdb.
//


//
// NOVAS_FSDB is internally used in NOVAS
//
#ifdef NOVAS_FSDB
#undef NOVAS_FSDB
#endif

#include <stdlib.h>
#include <argp.h>

#include "ffrAPI.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libgen.h>


/* Program documentation. */
static char doc[] =
  "Argp example #3 -- a program with options and arguments using argp";

/* A description of the arguments we accept. */
static char args_doc[] = "ARG1 ARG2";

/* The options we understand. */
static struct argp_option options[] = {
  {"fsdb",        'f', "waveform.fsdb",         0,  "RTL waveform file" },
  {"idcodes",     'i', "idcodes.txt",           0,  "List of FSDB idcodes to dump" },
//   {"sigwidths",   'w', "sigwidths.txt",         0,  "List of FSDB signal bit widths" },
  {"out",         'o', "out_toggles.bin",       0,  "Output file" },
  {"start",       's', "start_time_ns",         0,  "Start time (ns)" },
  {"end",         'e', "end_time_ns",           0,  "End time (ns)" },
  {"num_toggles", 'n', "clk_toggles/window",    0,  "Number of clock toggles per window" },
  { 0 }
};

/* Used by main to communicate with parse_opt. */
struct arguments
{
  char *fpath_fsdb;
  char *fpath_idcodes;
//   char *fpath_sigwidths;
  char *fpath_toggles;
  ulong_T start_time;
  ulong_T end_time;
  uint_T num_toggles;
};

/* Parse a single option. */
static error_t
parse_opt (int key, char *arg, struct argp_state *state)
{
  /* Get the input argument from argp_parse, which we
     know is a pointer to our arguments structure. */
  struct arguments *arguments = (struct arguments*) state->input;

  switch (key)
    {
    case 'f':
      arguments->fpath_fsdb = arg;
      break;
    case 'i':
      arguments->fpath_idcodes = arg;
      break;
    // case 'w':
    //   arguments->fpath_sigwidths = arg;
    //   break;
    case 'o':
      arguments->fpath_toggles = arg;
      break;
    case 's':
      arguments->start_time = atoi(arg);
      break;
    case 'e':
      arguments->end_time = atoi(arg);
      break;
    case 'n':
      arguments->num_toggles = atoi(arg);
      break;
    default:
      return ARGP_ERR_UNKNOWN;
    }
  return 0;
}

/* Our argp parser. */
static struct argp argp = { options, parse_opt, args_doc, doc };


#ifndef FALSE
#define FALSE	0
#endif

#ifndef TRUE
#define TRUE	1
#endif

// #define PRINT_CHANGE
#define PRINT_ARRAY

uint_T CLOCK_PERIOD = 4;
uint_T TIMESCALE = 1000; // timescale 1ns/10ps
uint_T MAX_TIME=0;
uint_T N_ROWS;
uint_T M_COLS;
int i;

// #define BUFFER_TYPE uint_T
#define BUFFER_TYPE unsigned short int
// BUFFER_TYPE* ones_buff = (BUFFER_TYPE*) malloc(BUFFER_SIZE*sizeof(BUFFER_TYPE));
BUFFER_TYPE* ones_buff;

uint_T* idcodes_buff;
uint_T idcode;
// uint_T* sigwidths_buff;
uint_T sigwidth, msb, bit_idx;

//
// The tree callback function, it's used to traverse the design 
// hierarchies. 
//
static bool_T __MyTreeCB(fsdbTreeCBType cb_type, 
			 void *client_data, void *tree_cb_data);


//
// dump scope definition
//
static void 
__DumpScope(fsdbTreeCBDataScope *scope);


//
// dump var definition 
// 
static void 
__DumpVar(fsdbTreeCBDataVar *var);


static void 
__DumpVar_less(fsdbTreeCBDataVar *var);

static bool_T 
__ValChng(ffrVCTrvsHdl vc_trvs_hdl, 
		   fsdbTag64 *time, byte_T *vc_ptr, 
           byte_T *prev_vc_ptr);

static void 
__PrintTimeValChng(ffrVCTrvsHdl vc_trvs_hdl, 
		   fsdbTag64 *time, byte_T *vc_ptr);

int 
main(int argc, char *argv[])
{
    struct arguments arguments;

    /* Default values. */
    arguments.fpath_fsdb = (char*) "-";
    arguments.fpath_idcodes = (char*) "-";
    // arguments.fpath_sigwidths = (char*) "-";
    arguments.fpath_toggles = (char*) "-";
    arguments.start_time = 0;
    arguments.end_time = 0;
    arguments.num_toggles = 100;

    /* Parse our arguments; every option seen by parse_opt will
        be reflected in arguments. */
    argp_parse (&argp, argc, argv, 0, 0, &arguments);

    printf ("fsdb_file= %s\nidcodes_file = %s\nbin_file = %s\n"
            "start_time = %lu\nend_time = %ld\nnum_toggles = %u\n",
            arguments.fpath_fsdb, arguments.fpath_idcodes, 
            // arguments.fpath_sigwidths,
            arguments.fpath_toggles,
            arguments.start_time, arguments.end_time, arguments.num_toggles);
    
    char *fpath_fsdb = arguments.fpath_fsdb;
    char *fpath_idcodes = arguments.fpath_idcodes;
    // char *fpath_sigwidths = arguments.fpath_sigwidths;
    char *fpath_toggles = arguments.fpath_toggles;
    ulong_T start_time = arguments.start_time;
    ulong_T end_time = arguments.end_time;

    ulong_T start_simtime = (ulong_T) start_time*TIMESCALE;
    
    uint_T N_CLOCK_CYCLES = arguments.num_toggles;
    uint_T SIMTIME2CYCLES = TIMESCALE*CLOCK_PERIOD;
    uint_T TW_L = N_CLOCK_CYCLES*SIMTIME2CYCLES;  // time window length


    if (FALSE == ffrObject::ffrIsFSDB(fpath_fsdb)) {
	fprintf(stderr, "%s is not an fsdb file.\n", fpath_fsdb);
	return FSDB_RC_FAILURE;
    }

    ffrFSDBInfo fsdb_info;

    ffrObject::ffrGetFSDBInfo(fpath_fsdb, fsdb_info);
    if (FSDB_FT_VERILOG != fsdb_info.file_type) {
  	fprintf(stderr, "file type is not verilog.\n");
	return FSDB_RC_FAILURE;
    }

    ffrObject *fsdb_obj =
	ffrObject::ffrOpen3(fpath_fsdb);
    if (NULL == fsdb_obj) {
	fprintf(stderr, "ffrObject::ffrOpen() failed.\n");
	exit(FSDB_RC_OBJECT_CREATION_FAILED);
    }
    fsdb_obj->ffrSetTreeCBFunc(__MyTreeCB, NULL);

    //
    // LOAD IDCODES FROM FILE
    // first value of idcodes file is the number of idcodes
    //
    FILE *f_idcodes = fopen(fpath_idcodes, "r");
    if (f_idcodes == NULL) {
        fprintf(stderr, "ERROR: File does not exist, %s\n", fpath_idcodes);
        return 0;
    }
    fscanf(f_idcodes, "%u ", &N_ROWS);
    idcodes_buff = (uint_T*) malloc(N_ROWS*sizeof(uint_T));
    i = 0;
    while (fscanf(f_idcodes, "%u ", &idcode) == 1) {
        idcodes_buff[i] = idcode;
        i += 1;
    }

    // //
    // // LOAD SIGWIDTHS FROM FILE
    // // first value of idcodes file is the number of idcodes
    // //
    // FILE *f_sigwidths = fopen(fpath_sigwidths, "r");
    // if (f_sigwidths == NULL) {
    //     fprintf(stderr, "ERROR: File does not exist, %s\n", fpath_sigwidths);
    //     return 0;
    // }
    // sigwidths_buff = (uint_T*) malloc(N_ROWS*sizeof(uint_T));
    // i = 0;
    // while (fscanf(f_sigwidths, "%u ", &sigwidth) == 1) {
    //     sigwidths_buff[i] = sigwidth;
    //     i += 1;
    // }

    
    FILE *f_toggles = fopen(fpath_toggles, "wb");
    if (f_toggles == NULL) {
        fprintf(stderr, "ERROR: File could not be created, %s\n", fpath_toggles);
        return 0;
    }

    if (FSDB_FT_VERILOG != fsdb_obj->ffrGetFileType()) {
	fprintf(stderr, 
		"%s is not verilog type fsdb, just return.\n", fpath_fsdb);
	fsdb_obj->ffrClose();
	return FSDB_RC_SUCCESS;
    }

    //
    // Activate the tree callback funciton, read the design 
    // hierarchies. Application has to perform proper type case 
    // on tree callback data based on the callback type, then uses 
    // the type case structure view to access the wanted data.
    //
    fsdb_obj->ffrReadScopeVarTree();
    fprintf(stderr, "Read design hierarchy.\n");

    //
    // Each unique var is represented by a unique idcode in fsdb 
    // file, these idcodes are positive integer and continuous from 
    // the smallest to the biggest one. So the maximum idcode also 
    // means that how many unique vars are there in this fsdb file. 
    //
    // Application can know the maximum var idcode by the following
    // API:
    //
    //		ffrGetMaxVarIdcode()
    //
    // fsdbVarIdcode max_var_idcode = fsdb_obj->ffrGetMaxVarIdcode();
    // id codes range from 1 - max_var_idcode
    // N_ROWS = (uint_T) max_var_idcode;  // number of rows = # variables

    //
    // In order to load value changes of vars onto memory, application
    // has to tell fsdb reader about what vars it's interested in. 
    // Application selects the interested vars by giving their idcodes
    // to fsdb reader, this is done by the following API:
    //
    //		ffrAddToSignalList()
    // 
    for (i = 0; i < N_ROWS; i++) {
        idcode = idcodes_buff[i];
        fsdb_obj->ffrAddToSignalList(idcode);
    }
    fprintf(stderr, "Added signals to list.\n");
    	

    //
    // Load the value changes of the selected vars onto memory. Note 
    // that value changes of unselected vars are not loaded.
    //
    fsdb_obj->ffrLoadSignals();
    fprintf(stderr, "Loaded signals.\n");

    //
    // In order to traverse the value changes of a specific var,
    // application must create a value change traverse handle for 
    // that sepcific var. Once the value change traverse handle is 
    // created successfully, there are lots of traverse functions 
    // available to traverse the value changes backward and forward, 
    // or jump to a sepcific time, etc.
    //
    ffrVCTrvsHdl vc_trvs_hdl;

    fsdbTag64 time;
    int	      glitch_num;
    byte_T    *vc_ptr;

    uint_T cntr = 0;


    ulong_T max_time_H = 0;
    ulong_T max_time_L = 0;
    
    // if (end_time == 0) {
    ulong_T sim_end_time = 0;
    // TODO: idk if we need to iterate over all signals, or is first one sufficient? bc doesn't every signal switch to X...?
    idcode = FSDB_MIN_VAR_IDCODE;

    vc_trvs_hdl = fsdb_obj->ffrCreateVCTraverseHandle(idcode);
        if (NULL == vc_trvs_hdl) {
        fprintf(stderr, "Failed to create a traverse handle(%u)\n", 
            idcode);
        exit(FSDB_RC_OBJECT_CREATION_FAILED);
        }
    // Get the maximim time(xtag) where has value change. 
    vc_trvs_hdl->ffrGetMaxXTag((void*)&time);
    if ((time.H > max_time_H) ||
        ((time.H == max_time_H) && (time.L > max_time_L))
        )
    {
        max_time_H = time.H;
        max_time_L = time.L;
    }
    // }
    sim_end_time = ((max_time_H << 32) + max_time_L)/TIMESCALE;
    if ((sim_end_time < end_time) || (end_time == 0)) end_time = sim_end_time;
    // }
    fprintf(stderr, "End time: %lu\n",end_time);
    // fprintf(stderr, "End time parts: %u %u\n",max_time_H, max_time_L);

    ulong_T end_simtime = (ulong_T) end_time*TIMESCALE;
    
    M_COLS = (uint_T) (end_time - start_time)/N_CLOCK_CYCLES/CLOCK_PERIOD;  // # columns = # time entries

    fprintf(stderr, "Number of signals to process: %u\n", N_ROWS);
    fprintf(stderr, "Number of time windows: %u\n", M_COLS);
    fprintf(stderr, "Array shape: (%u,%u)\n", 
            N_ROWS, M_COLS);
    

    ones_buff = (BUFFER_TYPE*) malloc(M_COLS*sizeof(BUFFER_TYPE));
    

    // since we know both N and M, write these to file first
    // write NxM array dimensions
    // need 4 bytes bc N_ROWS could exceed max value of uint16
    
    fwrite(&N_ROWS, sizeof(uint_T), 1, f_toggles);
    fwrite(&M_COLS, sizeof(uint_T), 1, f_toggles);
    
    //
    // We have traversed the value changes associated with the var 
    // whose idcode is max_var_idcode, now we are going to traverse
    // the value changes of the other vars. The idcode of the other
    // vars is from FSDB_MIN_VAR_IDCODE, which is 1, to 
    // max_var_idcode 
    //
    
    fprintf(stderr, "Reading idcodes from: %s\n", fpath_idcodes);
    fprintf(stderr, "Writing to file: %s\n", fpath_toggles);

    uint_T idx = 0;
    for (i = 0; i < N_ROWS; i++) {
        idcode = idcodes_buff[i];

        // dump LSB --> MSB (e.g. 0 upto 31)
        // msb = sigwidths_buff[i];
        // for (bit_idx=0; bit_idx <= msb; bit_idx++) {
        bit_idx = 0;

            // create a value change traverse handle associated with
            // current traversed var.
            vc_trvs_hdl = fsdb_obj->ffrCreateVCTraverseHandle(idcode);
                if (NULL == vc_trvs_hdl) {
                fprintf(stderr, "Failed to create a traverse handle(%u)\n", 
                    idcode);
                exit(FSDB_RC_OBJECT_CREATION_FAILED);
                }

            // Get the minimum time(xtag) where has value change. 
            vc_trvs_hdl->ffrGetMinXTag((void*)&time);

            // Jump to the minimum time(xtag). 
            vc_trvs_hdl->ffrGotoXTag((void*)&time);

            // Traverse all the value changes from the minimum time
            // to the maximum time
            uint_T num_ones = 0;
            byte_T prev_bit_val = FSDB_BT_VCD_X;
            byte_T curr_bit_val = FSDB_BT_VCD_X;

            ulong_T time_window = TW_L + start_simtime;
            ulong_T current_vc_simtime = 0;
            ulong_T prev_vc_simtime = 0;

            idx = 0;
            // zero out buffer
            memset(ones_buff, (BUFFER_TYPE) 0, M_COLS*sizeof(BUFFER_TYPE));
            do {
                
                vc_trvs_hdl->ffrGetXTag(&time);
                vc_trvs_hdl->ffrGetVC(&vc_ptr);
                curr_bit_val = vc_ptr[bit_idx];

                current_vc_simtime = ((ulong_T) time.H << 32) + time.L;
                if (current_vc_simtime <= start_simtime) {
                    num_ones = 0;
                } else if (current_vc_simtime > end_simtime) {
                    break;
                } else if (current_vc_simtime >= time_window) {
                    ones_buff[idx] = (BUFFER_TYPE) num_ones;
                    // bump up time_window to point to current time window
                    while (current_vc_simtime >= time_window) {
                        if (prev_bit_val == FSDB_BT_VCD_1) {
                            // prev val was 1 --> signal stayed 1 this entire window
                            num_ones += (uint_T) 2*(time_window - prev_vc_simtime)/SIMTIME2CYCLES;
                            ones_buff[idx] = (BUFFER_TYPE) num_ones;
                        }
                        prev_vc_simtime = time_window;
                        time_window += TW_L; idx += 1;
                        num_ones = 0;
                    }
                    if (idx > M_COLS) {
                        fprintf(stderr, "ERROR: index exceeds number of columns (idx: %u, curr_simtime: %u, end_simtime: %u)\n", 
                                idx, (uint_T) current_vc_simtime, end_simtime);
                        return 0;
                    }
                }
                if (prev_bit_val == FSDB_BT_VCD_1)  num_ones += (uint_T) 2*(current_vc_simtime - prev_vc_simtime)/SIMTIME2CYCLES;
                prev_vc_simtime = current_vc_simtime;
                prev_bit_val = curr_bit_val;
                
            } while(FSDB_RC_SUCCESS == vc_trvs_hdl->ffrGotoNextVC());
            
            // NOTE (nk): technically we throw out last incomplete time window

            // free this value change traverse handle
            vc_trvs_hdl->ffrFree();

            // write data to buffer
            fwrite(ones_buff, sizeof(BUFFER_TYPE), M_COLS, f_toggles);  

        // }
    }
    

    free(ones_buff);
    
    fclose(f_toggles);

    fsdb_obj->ffrResetSignalList();

    fsdb_obj->ffrUnloadSignals();


    fsdb_obj->ffrClose();
    return 0;
}

static bool_T 
__ValChng(ffrVCTrvsHdl vc_trvs_hdl, 
		   fsdbTag64 *time, byte_T *vc_ptr, 
           byte_T *prev_vc_ptr)
{ 
    static byte_T buffer[FSDB_MAX_BIT_SIZE+1];
    byte_T        *ret_vc;
    uint_T        i;
    fsdbVarType   var_type; 
    
    switch (vc_trvs_hdl->ffrGetBytesPerBit()) {
    case FSDB_BYTES_PER_BIT_1B:
        for (i = 0; i < vc_trvs_hdl->ffrGetBitSize(); i++) {
            if (vc_ptr[i] != prev_vc_ptr[i]) {
                return TRUE;
            }
        }
        return FALSE;
	break;

    case FSDB_BYTES_PER_BIT_4B:
        return FALSE;
        // var_type = vc_trvs_hdl->ffrGetVarType();
        // switch(var_type){
        // case FSDB_VT_VCD_MEMORY_DEPTH:
        // case FSDB_VT_VHDL_MEMORY_DEPTH:
	    // fprintf(stderr, "time: (%u %u)", time->H, time->L);
        //     fprintf(stderr, "  begin: %d", *((int*)vc_ptr));
        //     vc_ptr = vc_ptr + sizeof(uint_T);
        //     fprintf(stderr, "  end: %d\n", *((int*)vc_ptr));  
        //     break;
               
        // default:    
        //     vc_trvs_hdl->ffrGetVC(&vc_ptr);
	    // fprintf(stderr, "time: (%u %u)  val chg: %f\n",
        //             time->H, time->L, *((float*)vc_ptr));
	    // break;
        // }
        break;

    case FSDB_BYTES_PER_BIT_8B:
        return ((*((double*)vc_ptr)) != (*((double*)prev_vc_ptr)));
	break;
    }
    return FALSE;
}


static void 
__PrintTimeValChng(ffrVCTrvsHdl vc_trvs_hdl, 
		   fsdbTag64 *time, byte_T *vc_ptr)
{ 
    static byte_T buffer[FSDB_MAX_BIT_SIZE+1];
    byte_T        *ret_vc;
    uint_T        i;
    fsdbVarType   var_type; 
    
    switch (vc_trvs_hdl->ffrGetBytesPerBit()) {
    case FSDB_BYTES_PER_BIT_1B:

	//
 	// Convert each verilog bit type to corresponding
 	// character.
 	//
        for (i = 0; i < vc_trvs_hdl->ffrGetBitSize(); i++) {
	    switch(vc_ptr[i]) {
 	    case FSDB_BT_VCD_0:
	        buffer[i] = '0';
	        break;

	    case FSDB_BT_VCD_1:
	        buffer[i] = '1';
	        break;

	    case FSDB_BT_VCD_X:
	        buffer[i] = 'x';
	        break;

	    case FSDB_BT_VCD_Z:
	        buffer[i] = 'z';
	        break;

	    default:
		//
		// unknown verilog bit type found.
 		//
	        buffer[i] = 'u';
	    }
        }
        buffer[i] = '\0';
	fprintf(stderr, "time: (%u %u)  val chg: %s\n",
		time->H, time->L, buffer);	
	break;

    case FSDB_BYTES_PER_BIT_4B:
	//
	// Not 0, 1, x, z since their bytes per bit is
	// FSDB_BYTES_PER_BIT_1B. 
 	//
	// For verilog type fsdb, there is no array of 
  	// real/float/double so far, so we don't have to
	// care about that kind of case.
	//

        //
        // The var type of memory range variable is
        // FSDB_VT_VCD_MEMORY_DEPTH. This kind of var
        // has two value changes at certain time step.
        // The first value change is the index of the 
        // beginning memory variable which has a value change
        // and the second is the index of the end memory variable
        // which has a value change at this time step. The index
        // is stored as an unsigned integer and its bpb is 4B.
        //
        
        var_type = vc_trvs_hdl->ffrGetVarType();
        switch(var_type){
        case FSDB_VT_VCD_MEMORY_DEPTH:
        case FSDB_VT_VHDL_MEMORY_DEPTH:
	    fprintf(stderr, "time: (%u %u)", time->H, time->L);
            fprintf(stderr, "  begin: %d", *((int*)vc_ptr));
            vc_ptr = vc_ptr + sizeof(uint_T);
            fprintf(stderr, "  end: %d\n", *((int*)vc_ptr));  
            break;
               
        default:    
            vc_trvs_hdl->ffrGetVC(&vc_ptr);
	    fprintf(stderr, "time: (%u %u)  val chg: %f\n",
                    time->H, time->L, *((float*)vc_ptr));
	    break;
        }
        break;

    case FSDB_BYTES_PER_BIT_8B:
	//
	// Not 0, 1, x, z since their bytes per bit is
	// FSDB_BYTES_PER_BIT_1B. 
 	//
	// For verilog type fsdb, there is no array of 
  	// real/float/double so far, so we don't have to
	// care about that kind of case.
	//
	fprintf(stderr, "time: (%u %u)  val chg: %e\n",
		time->H, time->L, *((double*)vc_ptr));
	break;

    default:
	fprintf(stderr, "Control flow should not reach here.\n");
	break;
    }
}

static bool_T __MyTreeCB(fsdbTreeCBType cb_type, 
			 void *client_data, void *tree_cb_data)
{
    // nk - skip all of this for now
    return TRUE;
    switch (cb_type) {
    case FSDB_TREE_CBT_BEGIN_TREE:
	fprintf(stderr, "<BeginTree>\n");
	break;

    case FSDB_TREE_CBT_SCOPE:
	__DumpScope((fsdbTreeCBDataScope*)tree_cb_data);
	break;

    case FSDB_TREE_CBT_VAR:
	// __DumpVar((fsdbTreeCBDataVar*)tree_cb_data);
    __DumpVar_less((fsdbTreeCBDataVar*)tree_cb_data);
	break;

    case FSDB_TREE_CBT_UPSCOPE:
	fprintf(stderr, "<Upscope>\n");
	break;

    case FSDB_TREE_CBT_END_TREE:
	fprintf(stderr, "<EndTree>\n\n");
	break;

    case FSDB_TREE_CBT_FILE_TYPE:
	break;

    case FSDB_TREE_CBT_SIMULATOR_VERSION:
	break;

    case FSDB_TREE_CBT_SIMULATION_DATE:
	break;

    case FSDB_TREE_CBT_X_AXIS_SCALE:
	break;

    case FSDB_TREE_CBT_END_ALL_TREE:
	break;

    case FSDB_TREE_CBT_ARRAY_BEGIN:
        fprintf(stderr, "<BeginArray>\n");
        break;
        
    case FSDB_TREE_CBT_ARRAY_END:
        fprintf(stderr, "<EndArray>\n\n");
        break;

    case FSDB_TREE_CBT_RECORD_BEGIN:
        fprintf(stderr, "<BeginRecord>\n");
        break;
        
    case FSDB_TREE_CBT_RECORD_END:
        fprintf(stderr, "<EndRecord>\n\n");
        break;
             
    default:
	return FALSE;
    }

    return TRUE;
}

static void 
__DumpScope(fsdbTreeCBDataScope* scope)
{
    str_T type;

    switch (scope->type) {
    case FSDB_ST_VCD_MODULE:
	type = (str_T) "module"; 
	break;

    case FSDB_ST_VCD_TASK:
	type = (str_T) "task"; 
	break;

    case FSDB_ST_VCD_FUNCTION:
	type = (str_T) "function"; 
	break;

    case FSDB_ST_VCD_BEGIN:
	type = (str_T) "begin"; 
	break;

    case FSDB_ST_VCD_FORK:
	type = (str_T) "fork"; 
	break;

    default:
	type = (str_T) "unknown_scope_type";
	break;
    }

    fprintf(stderr, "<Scope> name:%s  type:%s\n", 
	    scope->name, type);
}

static void 
__DumpVar(fsdbTreeCBDataVar *var)
{
    str_T type;
    str_T bpb;

    switch(var->bytes_per_bit) {
    case FSDB_BYTES_PER_BIT_1B:
	bpb = (str_T) "1B";
	break;

    case FSDB_BYTES_PER_BIT_2B:
	bpb = (str_T) "2B";
	break;

    case FSDB_BYTES_PER_BIT_4B:
	bpb = (str_T) "4B";
	break;

    case FSDB_BYTES_PER_BIT_8B:
	bpb = (str_T) "8B";
	break;

    default:
	bpb = (str_T) "XB";
	break;
    }

    switch (var->type) {
    case FSDB_VT_VCD_EVENT:
	type = (str_T) "event"; 
  	break;

    case FSDB_VT_VCD_INTEGER:
	type = (str_T) "integer"; 
	break;

    case FSDB_VT_VCD_PARAMETER:
	type = (str_T) "parameter"; 
	break;

    case FSDB_VT_VCD_REAL:
	type = (str_T) "real"; 
	break;

    case FSDB_VT_VCD_REG:
	type = (str_T) "reg"; 
	break;

    case FSDB_VT_VCD_SUPPLY0:
	type = (str_T) "supply0"; 
	break;

    case FSDB_VT_VCD_SUPPLY1:
	type = (str_T) "supply1"; 
	break;

    case FSDB_VT_VCD_TIME:
	type = (str_T) "time";
	break;

    case FSDB_VT_VCD_TRI:
	type = (str_T) "tri";
	break;

    case FSDB_VT_VCD_TRIAND:
	type = (str_T) "triand";
	break;

    case FSDB_VT_VCD_TRIOR:
	type = (str_T) "trior";
	break;

    case FSDB_VT_VCD_TRIREG:
	type = (str_T) "trireg";
	break;

    case FSDB_VT_VCD_TRI0:
	type = (str_T) "tri0";
	break;

    case FSDB_VT_VCD_TRI1:
	type = (str_T) "tri1";
	break;

    case FSDB_VT_VCD_WAND:
	type = (str_T) "wand";
	break;

    case FSDB_VT_VCD_WIRE:
	type = (str_T) "wire";
	break;

    case FSDB_VT_VCD_WOR:
	type = (str_T) "wor";
	break;

    case FSDB_VT_VHDL_SIGNAL:
	type = (str_T) "signal";
	break;

    case FSDB_VT_VHDL_VARIABLE:
	type = (str_T) "variable";
	break;

    case FSDB_VT_VHDL_CONSTANT:
	type = (str_T) "constant";
	break;

    case FSDB_VT_VHDL_FILE:
	type = (str_T) "file";
	break;

    case FSDB_VT_VCD_MEMORY:
        type = (str_T) "vcd_memory";
        break;

    case FSDB_VT_VHDL_MEMORY:
        type = (str_T) "vhdl_memory";
        break;
        
    case FSDB_VT_VCD_MEMORY_DEPTH:
        type = (str_T) "vcd_memory_depth_or_range";
        break;
        
    case FSDB_VT_VHDL_MEMORY_DEPTH:         
        type = (str_T) "vhdl_memory_depth";
        break;

    default:
	type = (str_T) "unknown_var_type";
	break;
    }

    fprintf(stderr,
	"<Var>  name:%s  l:%u  r:%u  type:%s  ",
	var->name, var->lbitnum, var->rbitnum, type);
    fprintf(stderr,
	"idcode:%u  dtidcode:%u  bpb:%s\n",
	var->u.idcode, var->dtidcode, bpb);
}

static void 
__DumpVar_less(fsdbTreeCBDataVar *var)
{
    fprintf(stderr,
	"%u  %s\n",
	var->u.idcode, var->name);
    
}
