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
  {"idcode",      'i', "idcode",                0,  "FSDB idcode of signal todump" },
  {"out",         'o', "out_signal.bin",        0,  "Output file" },
  {"start",       's', "start_time_ns",         0,  "Start time (ns)" },
  {"end",         'e', "end_time_ns",           0,  "End time (ns)" },
  {"num_toggles", 'n', "clk_toggles/window",    0,  "Number of clock toggles per window" },
  { 0 }
};

/* Used by main to communicate with parse_opt. */
struct arguments
{
  char *fpath_fsdb;
  char *fpath_sigval;
  uint_T idcode;
  ulong_T start_time;
  ulong_T end_time;
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
      arguments->idcode = atoi(arg);
      break;
    case 'o':
      arguments->fpath_sigval = arg;
      break;
    case 's':
      arguments->start_time = atoi(arg);
      break;
    case 'e':
      arguments->end_time = atoi(arg);
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

#define BUFFER_TYPE unsigned char

uint_T TIMESCALE = 1000; // timescale 1ns/10ps

// The tree callback function, it's used to traverse the design 
// hierarchies. 
static bool_T __MyTreeCB(fsdbTreeCBType cb_type, 
			 void *client_data, void *tree_cb_data);


// dump scope definition
static void 
__DumpScope(fsdbTreeCBDataScope *scope);


// dump var definition 
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
    fprintf(stderr, "Size of uint_T: %u, Size of ulong_T: %u, Size of byte_T: %u\n",
         sizeof(uint_T), sizeof(ulong_T), sizeof(byte_T));
    struct arguments arguments;

    /* Default values. */
    arguments.fpath_fsdb = (char*) "-";
    arguments.idcode = 0;
    arguments.fpath_sigval = (char*) "-";
    arguments.start_time = 0;
    arguments.end_time = 0;

    /* Parse our arguments; every option seen by parse_opt will
        be reflected in arguments. */
    argp_parse (&argp, argc, argv, 0, 0, &arguments);

    printf ("fsdb_file= %s\nidcode = %u\nbin_file = %s\n"
            "start_time = %lu\nend_time = %ld\n",
            arguments.fpath_fsdb, arguments.idcode,
            arguments.fpath_sigval,
            arguments.start_time, arguments.end_time);
    
    char *fpath_fsdb = arguments.fpath_fsdb;
    uint_T idcode = arguments.idcode;
    char *fpath_sigval = arguments.fpath_sigval;
    ulong_T start_time = arguments.start_time;
    ulong_T end_time = arguments.end_time;

    fprintf(stderr, "Start time: %lu\n",start_time);
    ulong_T start_simtime = (ulong_T) start_time*TIMESCALE;

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

    ffrObject *fsdb_obj = ffrObject::ffrOpen3(fpath_fsdb);
    if (NULL == fsdb_obj) {
	fprintf(stderr, "ffrObject::ffrOpen() failed.\n");
	exit(FSDB_RC_OBJECT_CREATION_FAILED);
    }
    fsdb_obj->ffrSetTreeCBFunc(__MyTreeCB, NULL);
    
    FILE *f_sigval = fopen(fpath_sigval, "wb");
    if (f_sigval == NULL) {
        fprintf(stderr, "ERROR: File could not be created, %s\n", fpath_sigval);
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
    fsdb_obj->ffrAddToSignalList(idcode);
    fprintf(stderr, "Added signal to list.\n");

    //
    // Load the value changes of the selected vars onto memory. Note 
    // that value changes of unselected vars are not loaded.
    //
    fsdb_obj->ffrLoadSignals();
    fprintf(stderr, "Loaded signal.\n");

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
    
    if (end_time == 0) {
        vc_trvs_hdl = fsdb_obj->ffrCreateVCTraverseHandle(idcode);
        if (NULL == vc_trvs_hdl) { fprintf(stderr, "Failed to create a traverse handle(%u)\n", idcode); exit(FSDB_RC_OBJECT_CREATION_FAILED); }
        // Get the maximim time(xtag) where has value change. 
        vc_trvs_hdl->ffrGetMaxXTag((void*)&time);
        if ((time.H > max_time_H) ||
            ((time.H == max_time_H) && (time.L > max_time_L))
            )
        {
            max_time_H = time.H;
            max_time_L = time.L;
        }
        end_time = ((max_time_H << 32) + max_time_L)/TIMESCALE;
    }
    fprintf(stderr, "End time: %lu\n",end_time);
    ulong_T end_simtime = (ulong_T) end_time*TIMESCALE;

    fprintf(stderr, "Writing to file: %s\n", fpath_sigval);

    /*********************************
        TRAVERSE SINGLE SIGNAL 
    **********************************/

    // create a value change traverse handle associated with current traversed var.
    vc_trvs_hdl = fsdb_obj->ffrCreateVCTraverseHandle(idcode);
    if (NULL == vc_trvs_hdl) { fprintf(stderr, "Failed to create a traverse handle(%u)\n", idcode); exit(FSDB_RC_OBJECT_CREATION_FAILED); }

    // Get the minimum time(xtag) where has value change. 
    vc_trvs_hdl->ffrGetMinXTag((void*)&time);

    // Jump to the minimum time(xtag). 
    vc_trvs_hdl->ffrGotoXTag((void*)&time);

    uint_T bitwidth = vc_trvs_hdl->ffrGetBitSize();
    static uint_T buffer[FSDB_MAX_BIT_SIZE+1];
    fprintf(stderr, "Signal bitwidth: %u\n", bitwidth);

    fwrite(&bitwidth, sizeof(uint_T), 1, f_sigval);

    // Traverse all the value changes from the minimum time
    // to the maximum time.   
    ulong_T current_vc_simtime = 0;
    uint i;

    do {
        vc_trvs_hdl->ffrGetXTag(&time);
        vc_trvs_hdl->ffrGetVC(&vc_ptr);
        

        current_vc_simtime = ((ulong_T) time.H << 32) + time.L;
        if (current_vc_simtime <= start_simtime) { continue; } 
        else if (current_vc_simtime > end_simtime) { break; } 
        else {
            // __PrintTimeValChng(vc_trvs_hdl, &time, vc_ptr);
            // for (i = 0; i < vc_trvs_hdl->ffrGetBitSize(); i++) {
            //     if (vc_ptr[i] != FSDB_BT_VCD_1)  vc_ptr[i] = FSDB_BT_VCD_0;
            // }
            // write data to buffer
            // fwrite(&current_vc_simtime, sizeof(ulong_T), 1, f_sigval);
            fwrite(&current_vc_simtime, sizeof(ulong_T), 1, f_sigval);
            fwrite(vc_ptr, sizeof(byte_T), bitwidth, f_sigval);
            // fwrite(buffer, sizeof(uint_T), bitwidth, f_sigval);
        }
    } while(FSDB_RC_SUCCESS == vc_trvs_hdl->ffrGotoNextVC());
    
    // NOTE (nk): technically we throw out last incomplete time window

    // free this value change traverse handle
    vc_trvs_hdl->ffrFree();

    // free(signal_buff);
    
    fclose(f_sigval);

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
