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
  {"idcodes",     'i', "idcodes.txt",           0,  "List of FSDB idcodes to dump" },
  {"out",         'o', "out_toggles.bin",       0,  "Output file" },
  {"start",       's', "start_time_ns",         0,  "Start time (ns)" },
  {"end",         'e', "end_time_ns",           0,  "End time (ns)" },
  {"windowlen",   'w', "windowlen_ns",          0,  "Length of each window (ns)" },
  {"embedding",   'm', "embedding_method",      0,  "One of: toggles, ones" },
  {"verbose",     'v', 0,                       0,  "Verbose output" },
  { 0 }
};

/* Used by main to communicate with parse_opt. */
struct arguments
{
  char *fpath_fsdb;
  char *fpath_idcodes;
  char *fpath_toggles;
  ulong_T start_time;
  ulong_T end_time;
  uint_T windowlen;
  char *embedding;
  bool verbose;
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
    case 'o':
      arguments->fpath_toggles = arg;
      break;
    case 's':
      arguments->start_time = atoi(arg);
      break;
    case 'e':
      arguments->end_time = atoi(arg);
      break;
    case 'w':
      arguments->windowlen = atoi(arg);
      break;
    case 'm':
      arguments->embedding = arg;
      break;
    case 'v':
      arguments->verbose = 1;
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

// #define BUFFER_TYPE uint_T
#define BUFFER_TYPE unsigned short int

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


int 
main(int argc, char *argv[])
{
    struct arguments arguments;

    /* Default values. */
    arguments.fpath_fsdb = (char*) "-";
    arguments.fpath_idcodes = (char*) "-";
    arguments.fpath_toggles = (char*) "-";
    arguments.start_time = 0;
    arguments.end_time = 0;
    arguments.windowlen = 4000;
    arguments.verbose = false;

    /* Parse our arguments; every option seen by parse_opt will
        be reflected in arguments. */
    argp_parse (&argp, argc, argv, 0, 0, &arguments);

    printf ("fsdb_file = %s\nidcodes_file = %s\nbin_file = %s\n"
            "start_time = %lu\nend_time = %ld\nwindowlen = %u\nembedding = %s\n",
            arguments.fpath_fsdb, arguments.fpath_idcodes,
            arguments.fpath_toggles,
            arguments.start_time, arguments.end_time,
            arguments.windowlen, arguments.embedding);
    
    char *fpath_fsdb = arguments.fpath_fsdb;
    char *fpath_idcodes = arguments.fpath_idcodes;
    char *fpath_toggles = arguments.fpath_toggles;
    ulong_T start_time_ns = arguments.start_time;
    ulong_T end_time_ns = arguments.end_time;
    bool verbose = arguments.verbose;
    uint_T WINDOWLEN = arguments.windowlen;
    char *embedding = arguments.embedding;

    // READ FSDB FILE
    if (FALSE == ffrObject::ffrIsFSDB(fpath_fsdb)) {   fprintf(stderr, "%s is not an fsdb file.\n", fpath_fsdb);    return FSDB_RC_FAILURE; }

    ffrFSDBInfo fsdb_info;
    ffrObject::ffrGetFSDBInfo(fpath_fsdb, fsdb_info);
    if (FSDB_FT_VERILOG != fsdb_info.file_type) {   fprintf(stderr, "file type is not verilog.\n");	return FSDB_RC_FAILURE;    }

    ffrObject *fsdb_obj = ffrObject::ffrOpen3(fpath_fsdb);
    if (NULL == fsdb_obj) { 	fprintf(stderr, "ffrObject::ffrOpen() failed.\n");	exit(FSDB_RC_OBJECT_CREATION_FAILED);    }
    fsdb_obj->ffrSetTreeCBFunc(__MyTreeCB, NULL);


    // GET TIMESCALE
    str_T scale_unit = fsdb_obj->ffrGetScaleUnit();
    uint_T digit;
    char   *unit;
    ffrObject::ffrExtractScaleUnit(scale_unit, digit, unit);
    fprintf(stderr, "SCALE = %u, unit = %s\n", digit, unit);
    uint_T TIMESCALE;
    if      (strcmp(unit,"ns")==0)  TIMESCALE = 1;
    else if (strcmp(unit,"ps")==0)  TIMESCALE = 1000;
    else if (strcmp(unit,"fs")==0)  TIMESCALE = 1000000;
    else { fprintf(stderr, "Unsupported FSDB scale unit: %s.\n", unit); exit(1); }
    TIMESCALE *= digit;
    fprintf(stderr, "FSDB timescale: %u%s, _ ns x %u --> <timescale>\n", digit,unit,TIMESCALE);

    ulong_T start_simtime = (ulong_T) start_time_ns * TIMESCALE;
    ulong_T end_simtime   = (ulong_T) end_time_ns * TIMESCALE;

    // EMBEDDING TYPE
    bool_T emb_toggles, emb_ones, emb_trans = FALSE;
    if (strcmp(embedding,"toggles") == 0) emb_toggles = TRUE;
    else if (strcmp(embedding,"ones") == 0) emb_ones = TRUE;
    else if (strcmp(embedding,"trans") == 0) emb_trans = TRUE;
    else {
        fprintf(stderr, "Unsupported signal embedding value: %s\n", embedding);
        exit(1);
    }
    fprintf(stderr, "Embeddings: toggles-%u, ones-%u, trans-\n", emb_toggles,emb_ones,emb_trans);

    // START/END SIM TIMES
    fsdbTag64 current_tagtime,start_tagtime,end_tagtime;
    start_tagtime.L = (uint_T) start_simtime;
    start_tagtime.H = (uint_T) (start_simtime >> 32);
    fsdb_obj->ffrGetMaxFsdbTag64(&end_tagtime);
    ulong_T sim_end_simtime = ((ulong_T) end_tagtime.H << 32) + end_tagtime.L;
    if ((sim_end_simtime < end_simtime) || (end_simtime == 0)) end_simtime = sim_end_simtime;

    uint_T TW_L = WINDOWLEN * TIMESCALE;  // time window length

    if (verbose) {
        fprintf(stderr, "Start time: %lu %s\n", start_simtime,unit);
        fprintf(stderr, "End time:   %lu %s\n",   end_simtime,unit);
        fprintf(stderr, "Time window length:   %u %s\n", TW_L,unit);
    }


    FILE *f_idcodes = fopen(fpath_idcodes, "r");
    if (f_idcodes == NULL) {
        fprintf(stderr, "ERROR: File does not exist, %s\n", fpath_idcodes);
        return 0;
    }

    // LOAD IDCODES FROM FILE
    // first value of idcodes file is the number of idcodes
    uint_T N_ROWS;
    fscanf(f_idcodes, "%u ", &N_ROWS);
    uint_T* idcodes_buff = (uint_T*) malloc(N_ROWS*sizeof(uint_T));
    uint_T i = 0; uint_T idcode;
    while (fscanf(f_idcodes, "%u ", &idcode) == 1) {
        idcodes_buff[i] = idcode;
        i += 1;
    }

    // ALLOCATE EMBEDDINGS BUFFER
    ulong_T M_COLS = (ulong_T) ((float) end_simtime - start_simtime)/((float) TW_L);  // # columns = # time entries
    if (verbose) fprintf(stderr, "Number of signals to process: %u\n", N_ROWS);
    if (verbose) fprintf(stderr, "Number of time windows: %u\n", M_COLS);
    if (verbose) fprintf(stderr, "Array shape: (%u,%u)\n", N_ROWS, M_COLS);
    BUFFER_TYPE* buff_sigembs = (BUFFER_TYPE*) malloc(M_COLS*sizeof(BUFFER_TYPE));


    if (FSDB_FT_VERILOG != fsdb_obj->ffrGetFileType()) { fprintf(stderr, "%s is not verilog type fsdb, just return.\n", fpath_fsdb); fsdb_obj->ffrClose(); return FSDB_RC_SUCCESS; }


    fsdb_obj->ffrReadScopeVarTree();
    if (verbose) fprintf(stderr, "Read design hierarchy.\n");


    // ADD ALL SIGNALS TO LIST
    for (i = 0; i < N_ROWS; i++) {
        idcode = idcodes_buff[i];
        fsdb_obj->ffrAddToSignalList(idcode);
    }
    if (verbose) fprintf(stderr, "Added signals to list.\n");
    	

    fsdb_obj->ffrLoadSignals();
    if (verbose) fprintf(stderr, "Loaded signals.\n");
    

    int	      glitch_num;
    byte_T    *vc_ptr;

    uint_T cntr = 0;


    FILE *f_toggles = fopen(fpath_toggles, "wb");
    if (f_toggles == NULL) {
        fprintf(stderr, "ERROR: File could not be created, %s\n", fpath_toggles);
        return 0;
    }


    
    if (verbose) fprintf(stderr, "Reading idcodes from: %s\n", fpath_idcodes);
    if (verbose) fprintf(stderr, "Writing to file: %s\n", fpath_toggles);

    uint_T sig_bitwidth;
    BUFFER_TYPE val_sigemb;
    ulong_T val_ones;

    ffrVCTrvsHdl vc_trvs_hdl;
    
    for (i = 0; i < N_ROWS; i++) {
        idcode = idcodes_buff[i];
        // create a value change traverse handle associated with
        // current traversed var.
        vc_trvs_hdl = fsdb_obj->ffrCreateVCTraverseHandle(idcode);
            if (NULL == vc_trvs_hdl) {
            fprintf(stderr, "Failed to create a traverse handle(%u)\n", 
                idcode);
            exit(FSDB_RC_OBJECT_CREATION_FAILED);
            }

        vc_trvs_hdl->ffrGotoXTag((void*)&start_tagtime);
        vc_trvs_hdl->ffrGetVC(&vc_ptr);
        sig_bitwidth = vc_trvs_hdl->ffrGetBitSize();
        byte_T* prev_vc = (byte_T*) malloc(sig_bitwidth*sizeof(byte_T));
        // if (idcode == 18401) printf("sig_bitwidth %u\n",sig_bitwidth);

        ulong_T time_window = TW_L + start_simtime;
        ulong_T prev_vc_simtime, current_vc_simtime = 0;

        uint_T idx_sigemb = 0;
        uint_T ibit;
        // zero out buffer
        memset(buff_sigembs, (BUFFER_TYPE) 0, M_COLS*sizeof(BUFFER_TYPE));
        val_sigemb = 0; val_ones = 0;
        do {
            vc_trvs_hdl->ffrGetXTag(&current_tagtime);
            prev_vc_simtime = current_vc_simtime;
            current_vc_simtime = ((ulong_T) current_tagtime.H << 32) + current_tagtime.L;
            // copy vals to old vc ptr
            for (ibit=0; ibit<sig_bitwidth; ibit++) {
                prev_vc[ibit] = vc_ptr[ibit];
            }
            vc_trvs_hdl->ffrGetVC(&vc_ptr);
            if (current_vc_simtime <= start_simtime) {
                continue;
            } else if ((end_simtime != 0) && (current_vc_simtime > end_simtime)) {
                break;
            } else if (current_vc_simtime >= time_window) {
                // if (idcode == 18404) printf("1 %lu %lu %lu %u %u \n",prev_vc_simtime,time_window,val_ones,TW_L,val_sigemb);
                // if (idcode == 18404) printf("1 %lu %lu %lu %u %u \n",prev_vc_simtime,time_window,val_ones,TW_L,val_sigemb);
                // bump up time_window to point to current time window
                while (current_vc_simtime >= time_window) {
                    if (emb_ones) {
                        for (ibit=0; ibit<sig_bitwidth; ibit++) {
                            if (prev_vc[ibit] == FSDB_BT_VCD_1) val_ones += time_window - prev_vc_simtime;
                        }
                        val_sigemb = (BUFFER_TYPE)((float) val_ones / ((float)TW_L/1000.0));
                        // if (idcode == 18404) printf("2 %lu %lu %lu %u %u \n",prev_vc_simtime,time_window,val_ones,TW_L,val_sigemb);
                    }
                    // if (idcode == 18404) printf("%lu %lu %u \n",prev_vc_simtime,time_window,val_sigemb);
                    buff_sigembs[idx_sigemb] = val_sigemb;
                    val_ones = 0; val_sigemb = 0;
                    prev_vc_simtime = time_window;
                    time_window += TW_L; idx_sigemb += 1;
                }
                if (idx_sigemb > M_COLS) { fprintf(stderr, "ERROR: index exceeds number of columns (idx: %u, curr_simtime: %lu)\n", idx_sigemb, current_vc_simtime); return 0; }
            }
            for (ibit=0; ibit<sig_bitwidth; ibit++) {
                // if (idcode == 18408) printf("idx=%u,b0 vs b1 %u %u\n",idx_sigemb,prev_vc[ibit],vc_ptr[ibit]);
                if (emb_trans) {
                    val_sigemb = 1;
                }
                else if (emb_toggles && (prev_vc[ibit] != vc_ptr[ibit])) {
                    val_sigemb++;
                }
                else if (emb_ones && (prev_vc[ibit] == FSDB_BT_VCD_1))
                {
                    val_ones += current_vc_simtime - prev_vc_simtime;
                }
                    
            }
        } while(FSDB_RC_SUCCESS == vc_trvs_hdl->ffrGotoNextVC());
        // NOTE (nk): technically we throw out last incomplete time window
        // free this value change traverse handle
        vc_trvs_hdl->ffrFree();

        free(prev_vc);

        // write data to buffer
        fwrite(buff_sigembs, sizeof(BUFFER_TYPE), M_COLS, f_toggles);
    }
    
    free(buff_sigembs);
    
    fclose(f_toggles);

    fsdb_obj->ffrResetSignalList();

    fsdb_obj->ffrUnloadSignals();


    fsdb_obj->ffrClose();
    return 0;
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
