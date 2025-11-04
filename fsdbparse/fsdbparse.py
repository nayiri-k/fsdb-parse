import os, sys
from pathlib import Path
import scipy.sparse
import subprocess
from typing import List
import numpy as np
import pandas as pd
import struct
from datetime import datetime, timedelta


from fsdbparse import util
from fsdbparse.globalvars import *


class FSDBParse():

    fsdbparse_dpath = Path(__file__).parent

    def setup_env():
        os.environ['VERDI_VERSION'] = VERDI_VERSION
        os.environ['LD_LIBRARY_PATH'] = f"{VERDI_HOME}/share/FsdbReader/linux64:/opt/rh/devtoolset-8/root/usr/lib64"


    def dump_header(fsdb_fpath,header_fpath,verbose=False) -> None:
        FSDBParse.setup_env()
        cmd = f"{FSDBParse.fsdbparse_dpath}/print_header {fsdb_fpath} > {header_fpath}"
        if verbose: print(cmd)
        os.system(cmd)


    def get_dasm(sigvals: list):
        to_print = '\n'.join([f"DASM({s})" for s in sigvals])
        with open('tmp-inst.log','w') as f: f.write(to_print)
        # Execute the first command and pipe its output to spike-dasm
        process = subprocess.Popen(["cat", "tmp-inst.log"], stdout=subprocess.PIPE)
        spike_dasm = f"{chipyard_dir}/.conda-env/riscv-tools/bin/spike-dasm"
        spike_dasm_process = subprocess.Popen([spike_dasm], stdin=process.stdout, stdout=subprocess.PIPE)
        # Capture the output of spike-dasm
        output, _ = spike_dasm_process.communicate()
        # Convert the output bytes to a string
        output_str = output.decode("utf-8").strip()
        dasm = output_str.split('\n')
        return dasm


    def get_endtime(fp):
        return FSDBParse.get_fsdbinfo(fp)['end_ns']


    def get_fsdbinfo(fp: str = None):
        # this cmd sometimes freezes, so give it a timeout
        cmd = f"timeout 3 {VERDI_HOME}/bin/fsdbdebug -info {fp}"
        FSDBParse.setup_env()
        info = subprocess.run(cmd,shell=True,capture_output=True,text=True)
        s = info.stderr
        key2val = {l.split(':')[0].strip(): l.split(':')[-1].strip() for l in s.split('\n') if ':' in l}
        endtime = key2val['maximum xtag'].split()[-1].replace('(','').replace(')','')
        d = {}
        d['endtime'] = endtime
        d['end_ns'] = int(util.to_ns(endtime))
        return {**d,**key2val}


    def get_siginfo_subset(signames,siginfo):
        signames = [sn.replace('.','/') for sn in signames]
        sn2id = {sn.split('[')[0]: idcode for idcode,slist in zip(siginfo.index,siginfo['signame']) for sn in slist.split(',')}
        ids = [sn2id[sn] for sn in signames]
        df = siginfo.loc[ids]
        return df.loc[~df.index.duplicated(keep='first')].sort_index()



class FSDBData():
    def __init__(self,
                    fsdb_fpath: str,  # path to FSDB waveform
                    rtl: str,  # name of RTL design
                    name: str = None, # name of this run (usually workload name)
                    windowlen: int = None,
                    hd_bitwidth: int = 8,
                    start_time: int = 0,
                    end_time: int = 0,
                    intervals: list = None,  # NOT IMPLEMENTED
                    cache_dpath: Path = Path.cwd()/"out",
                    sigset: str = None,
                    embedding: str = 'toggles', # one of: toggles, ones,
                    name_ext: str = '',
                    overwrite: bool = False,
                    print_only: bool = False,
                    verbose: bool = False,
                    ):
        self.fsdb_fpath = Path(fsdb_fpath)
        if name is None: name = self.fsdb_fpath.stem
        self.rtl = rtl
        self.name = name
        self.cache_dpath = Path(cache_dpath)
        self.windowlen = int(windowlen)
        self.overwrite = overwrite
        self.print_only = print_only
        self.verbose = verbose
        self.start_time = int(start_time)
        self.end_time = int(end_time)
        self.intervals = Path(intervals) if intervals else False
        self.embedding = embedding
        feature_ext_dict = dict(toggles='t',ones='o')
        self.feature_ext = feature_ext_dict[self.embedding]


        if len(name_ext) > 0 and name_ext[0] != '.': name_ext = '.'+name_ext
        name_ext += f".windowlen-{self.windowlen}"
        if name_ext in name: name_ext = ""
        if self.start_time: name_ext += f".start-{self.start_time}"
        if self.end_time:   name_ext += f".end-{self.end_time}"

        self.fpaths = {}
        self.fpaths['header'] = self.cache_dpath/f"headers/{rtl}.txt"

        dirname = rtl
        self.sigset = sigset
        if sigset is not None: dirname += '.sigs-' + sigset

        self.fpaths['siginfo_stem'] = self.cache_dpath/"siginfo"/dirname
        self.fpaths['emb_bin'] = self.cache_dpath/self.embedding/dirname/f"{name}{name_ext}.bin"
        self.fpaths['emb_csc'] = self.cache_dpath/self.embedding/dirname/f"{name}{name_ext}.npz"
        for fpath in self.fpaths.values():
            if fpath.suffix != '': fpath = fpath.parent
            fpath.mkdir(exist_ok=True,parents=True)
        # get siginfo
        fp = self.fpaths['siginfo_stem']/"siginfo.h5"
        if not fp.exists(): self.generate_siginfo()
        self.siginfo = pd.read_hdf(fp,key='df')


    def generate_siginfo(self) -> None:
        if not self.fpaths['header'].exists():
            FSDBParse.dump_header(self.fsdb_fpath,self.fpaths['header'],verbose=self.verbose)

        signal_id_dict = {}
        id_width_dict = {}
        id_signal_dict = {}
        inst_hier = []
        inst_hier_path = ""
        inst_hier_path_chiptop = ""
        with self.fpaths['header'].open() as f: lines = f.readlines()
        for line in lines:
            words = line.split()
            if len(words) < 1: continue
            if words[0] == '<Scope>':
                name=words[1].replace('name:','')
                inst_hier.append(name)
                inst_hier_path_chiptop = '/'.join(['ChipTop']+inst_hier[3:])
                inst_hier_path = '/'.join(inst_hier)
            elif words[0] == '<Upscope>':
                inst_hier.pop(-1)
            elif words[0] == '<Var>':
                if not inst_hier_path.startswith('TestDriver/testHarness/chiptop'): continue
                # if inst_hier_path_chiptop not in instance_paths: continue
                idcode=int(words[2])
                # idcodes will repeat!
                # if idcode in id_signal_dict: continue
                signal=words[1]
                signal_name = signal.split('/')[-1]
                if signal_name.startswith('_GEN') or signal_name.startswith('_T'):
                # if signal_name.startswith('_'):
                    continue
                path = inst_hier_path
                sig_path=f"{path}/{signal}"
                if idcode not in id_signal_dict: id_signal_dict[idcode] = []
                id_signal_dict[idcode].append(sig_path)
                signal_id_dict[sig_path] = idcode
                if signal.endswith(':0]'):
                    width = signal.split('[')[-1]  # name[3][31:0] --> only keep 31:0]
                    width = int(width[:-3]) # remove :0]
                    id_width_dict[idcode] = width + 1
                else:
                    id_width_dict[idcode] = 1

        id_signals = sorted(id_signal_dict.items()) #[(i,s) for i,s in sorted(id_signal_dict.items())]
        ids = [id for id,_ in id_signals]
        sns = [','.join(ss) for _,ss in id_signals]
        sigwidths = [id_width_dict[id] for id in ids]
        df = pd.DataFrame()
        df['signame'] = sns
        df['idcode'] = ids
        df['sigwidth'] = sigwidths
        df = df.set_index('idcode')
        df = df.loc[~df.index.duplicated(keep='first')].sort_index()
        FSDBData.write_siginfo(df,self.fpaths['siginfo_stem'])


    def write_siginfo(df,stem):
        stem = Path(stem)
        stem.mkdir(exist_ok=True,parents=True)

        df = df.loc[~df.index.duplicated(keep='first')].sort_index()  # drop duplicate idcodes

        num_idcodes = len(df)

        siginfo_fpath = stem/'siginfo.h5'
        idcodes_fpath = stem/'idcodes.txt'
        sigwidths_fpath = stem/'sigwidths.txt'

        with open(idcodes_fpath,'w') as fid:
            with open(sigwidths_fpath,'w') as fsw:
                fid.write(f"{num_idcodes} ")
                for id,sigwidth in zip(df.index,df['sigwidth']):
                    fid.write(f"{id} ")
                    fsw.write(f"{sigwidth} ")
        
        df.to_hdf(siginfo_fpath,key='df',mode='w',complevel=9)


    def generate_siginfo_sigset(self, sigset: str, signames: list) -> pd.DataFrame:
        df = FSDBParse.get_siginfo_subset(signames,self.siginfo)
        self.write_siginfo_sigset(df,sigset)
        return df


    def write_siginfo_sigset(self,df: pd.DataFrame, sigset: str):
        stem = self.cache_dpath/f"siginfo/{self.rtl+'.sigs-'+sigset}"
        FSDBData.write_siginfo(df,stem)        


    def add_bit(s,b):
        bit_str = b if type(b) == str else f"-b_{b}"
        return s.replace(',',bit_str+',') + bit_str


    def merge_hd(self, df, merge_cols=None) -> pd.DataFrame:
        if self.hd_bitwidth <= 1: return df
        if self.verbose: print(f"Merging into {self.hd_bitwidth}-bit signals: ",end='')
        if self.verbose: print(f"Renaming columns...",end='')
        if merge_cols is None:
            signames = self.siginfo['signame']
            sigwidths = self.sigwidths['sigwidth']
            merge_cols = {FSDBData.add_bit(s,f"-b_{min(lsb + self.hd_bitwidth, w)-1}_{lsb}"):
                            [FSDBData.add_bit(s,b) for b in range(lsb,min(lsb + self.hd_bitwidth, w))] 
                                for s,w in zip(signames,sigwidths) if w > 1
                                    for lsb in range(0,w,self.hd_bitwidth)}
        if self.verbose: print(f"Getting columns to add...",end='')
        cols_to_add = {i: [l[i] for l in merge_cols.values() if i < len(l)] for i in range(0,self.hd_bitwidth)}
        if self.verbose: print(f"Creating mapper...",end='')
        mapper = {c: n for n,l in merge_cols.items() for c in l}
        if self.verbose: print(f"Renaming bit signals...",end='')
        # print(df.columns)
        df_hd = df.rename(mapper,axis=1)
        # df_hd = pd.concat([df[cols_to_add[i]].rename(mapper,axis=1) for i in range(self.hd_bitwidth)],axis=1)
        if self.verbose: print(f"Merging signals...",end='')
        df_hd = df_hd.groupby(df_hd.columns, axis=1).sum()
        if self.verbose: print(f"Re-adding single-bit signals...",end='')
        df = df[df.columns[~df.columns.str.contains('-b_')]]
        df = pd.concat([df, df_hd], axis=1)
        # if self.verbose: print("Done")
        return df
    
    # def merge_hd2(self, df) -> pd.DataFrame:
    #     if self.hd_bitwidth <= 1: return df
    #     if self.verbose: print(f"Merging into {self.hd_bitwidth}-bit signals: ",end='')
    #     if self.verbose: print(f"Renaming columns...",end='')
    #     merge_cols = {FSDBData.add_bit(s,f"-b_{min(lsb + self.hd_bitwidth, w)-1}_{lsb}"):
    #                     [FSDBData.add_bit(s,b) for b in range(lsb,min(lsb + self.hd_bitwidth, w))] 
    #                             for s,w in zip(self.signames,self.sigwidths) if w > 1
    #                                 for lsb in range(0,w,self.hd_bitwidth)}
    #     self.merge_cols = merge_cols
    #     if self.verbose: print(f"Getting columns to add...",end='')
    #     cols_to_add = {i: [l[i] for l in merge_cols.values() if i < len(l)] for i in range(0,self.hd_bitwidth)}
    #     if self.verbose: print(f"Creating mapper...",end='')
    #     mapper = {c: n for n,l in merge_cols.items() for c in l}
    #     if self.verbose: print(f"Initializing dataframes...",end='')
    #     cols = sorted(merge_cols.keys())
    #     # df_hd = pd.DataFrame(0, index=df.index,columns=cols, dtype=np.uint16)
    #     df2   = pd.DataFrame(columns=cols, dtype=np.uint16)
    #     if self.verbose: print(f"Merging signals...",end='')
    #     df_list = [df[cols_to_add[i]].rename(mapper,axis=1) for i in range(self.hd_bitwidth)]
    #     df_hd = pd.concat(df_list, axis=1).sum(axis=1)
    #     # for i in range(self.hd_bitwidth):
    #     #     if self.verbose: print(f"{i}...",end='')
    #     #     df2 = df[cols_to_add[i]].rename(mapper,axis=1)
    #     #     df_hd = df_hd.add(df2,fill_value=0) #.astype(np.uint16)
    #     if self.verbose: print(f"Re-adding single-bit signals...",end='')
    #     df = df[df.columns[~df.columns.str.contains('-b_')]]
    #     df = pd.concat([df, df_hd], axis=1)
    #     if self.verbose: print("Done")
    #     return df



    def dump_embedding(self, fsdb_fpath: str = None,
                             emb_bin_fpath: str = None) -> None:
        if fsdb_fpath is None: fsdb_fpath = self.fsdb_fpath
        if emb_bin_fpath is None: emb_bin_fpath = self.fpaths['emb_bin']

        if self.verbose: print(f"# Dumping {self.embedding} for {self.name}")
        idcodes_fpath = self.fpaths['siginfo_stem']/"idcodes.txt"
        if not idcodes_fpath.exists(): self.generate_siginfo()
        executable = 'dump_embedding'
        FSDBParse.setup_env()

        cmd =  str(FSDBParse.fsdbparse_dpath/"dump_embedding")
        cmd += f" --fsdb {fsdb_fpath}"
        cmd += f" --idcodes {idcodes_fpath}"
        cmd += f" --windowlen {self.windowlen}"
        cmd += f" --embedding {self.embedding}"
        if self.start_time: cmd += f" --start {self.start_time}"
        if self.end_time:   cmd += f" --end {self.end_time}"
        if self.intervals:  cmd += f" --intervals {str(self.intervals)}"
        if self.verbose:
            cmd += f" --verbose"
        cmd += f" --out {emb_bin_fpath}"
        if self.verbose or self.print_only:
            print(cmd)
        if not self.print_only:
            subprocess.run(cmd, shell=True, env=os.environ)

    def compress_embedding(self) -> None:
        bin_fpath = f = self.fpaths['emb_bin']
        if self.overwrite or not f.exists() or (f.exists() and os.stat(f).st_size < 100):
            self.dump_embedding(self.fsdb_fpath,f)
        if not bin_fpath.exists(): return
        toggles = None
        array = np.fromfile(bin_fpath,dtype=np.uint16)
        if len(array) == 0:
            print(f"WARNING: Empty file at {bin_fpath}")
            return
        # assert len(array) > 0, f"Empty array in {self.fpaths['emb_bin']}"
        N = len(self.siginfo) # M = int(len(array)/N)
        try:
            arr_reshape = array.reshape(N,-1)
        except ValueError:
            print(f"WARNING: Incomplete file (len={len(array)}, num rows={len(array)//N}) at {bin_fpath}")
            return
        w_embedding = np.transpose(arr_reshape)
        toggles = np.concatenate([toggles,w_embedding],axis=0) if toggles is not None else w_embedding
        os.remove(bin_fpath)
        if toggles is None: return
        if self.verbose: print(f"Writing compressed toggles w/ shape {toggles.shape} -> {str(self.fpaths['emb_csc'])}")
        sparse_matrix = scipy.sparse.csc_matrix(toggles)
        scipy.sparse.save_npz(self.fpaths['emb_csc'],sparse_matrix)


    def get_embedding(self, index=True, print_only=False) -> pd.DataFrame:
        self.print_only = print_only or self.print_only
        csc_fpath = self.fpaths['emb_csc']
        if not csc_fpath.exists() or self.overwrite: self.compress_embedding()
        if not csc_fpath.exists(): return pd.DataFrame()
        try: sparse_matrix = scipy.sparse.load_npz(csc_fpath)
        except:
            print(f"ERROR: CSC file is not a zip file, {str(csc_fpath)}")
            # os.remove(csc_fpath)
            return pd.DataFrame()
        w_embedding = sparse_matrix.toarray()
        if index:
            if self.intervals:
                intervals = util.load_vector(self.intervals)
                intervals = np.array([(s,e) for s,e in zip(intervals[::2],intervals[1::2])])
                intervals = intervals // self.windowlen
                windows = [i for s,e in intervals for i in range(s,e)]
            else:
                windows = range(w_embedding.shape[0])
            index = pd.Index([self.name+f".{i}" for i in windows],name='name.window')
        df = pd.DataFrame(w_embedding,index=index,columns=self.siginfo['signame'])
        # df = df[df.columns[~df.columns.str.contains("\[")]]
        if self.verbose: print(f"{self.name} {df.shape}...",end='')
        return df


