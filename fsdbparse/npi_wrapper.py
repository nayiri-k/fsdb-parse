import os, sys
from pathlib import Path
import scipy.sparse
import subprocess
from typing import List
import numpy as np
import pandas as pd
import struct
from datetime import datetime, timedelta

from fsdbparse.globalvars import *

class NPI():
    '''wrapper for Verdi NPI python functions'''

    def __init__(self,fsdb_fpath: str):
        fsdb_fpath = str(fsdb_fpath)

        # run npisys.init once
        try: npisys
        except NameError:
            os.environ["VERDI_HOME"] = VERDI_HOME
            os.environ["SNPSLMD_LICENSE_FILE"] = "1701@bwrcflex-1.eecs.berkeley.edu:1701@bwrcflex-2.eecs.berkeley.edu"
            rel_lib_path = os.environ["VERDI_HOME"] + "/share/NPI/python"
            sys.path.append(os.path.abspath(rel_lib_path))
            from pynpi import npisys
            import pynpi.waveform as waveform
            self.waveform = waveform
            npisys.init(sys.argv)

        fsdbfile = self.waveform.open(fsdb_fpath)
        assert fsdbfile is not None, "Opening FSDB file failed"
        ret = self.waveform.is_fsdb(fsdb_fpath)
        assert ret, "File is not an FSDB file"
        self.fsdbfile = fsdbfile


    def close(self):
        try: 
            self.waveform.close(self.fsdbfile)
        except: pass


    def get_all_sigs(self,top_scope=None):
        scopes = [self.fsdbfile.scope_by_name(top_scope)] if top_scope is not None else self.fsdbfile.top_scope_list()
        all_sigs = []
        while scopes:
            scope = scopes.pop(0)
            all_sigs += [s for s in scope.sig_list()]
            scopes += scope.child_scope_list()
        return all_sigs


    def scale_to_ns(self):
        return self.waveform.convert_time_out(self.fsdbfile,1,"ns")


    def get_siginfo(self, sigs):
        ranges = [(s.left_range(),s.right_range()) for s in sigs]
        widths = [max(se)-min(se)+1 for se in ranges]
        names = [s.name() for s in sigs]
        fullnames = [s.full_name() for s in sigs]
        return dict(ranges=ranges,widths=widths,names=names,fullnames=fullnames)


    def load_sigs_by_names(self,signames):
        # load all signals
        self.fsdbfile.reset_sig_list()
        sigs = []
        for sn in signames:
            sig = self.fsdbfile.sig_by_name(sn)
            self.fsdbfile.add_to_sig_list(sig)
            sigs.append(sig)
        self.loaded_sigs = sigs
        return sigs


    def embed_signal(self,sn,window_ns,embedding='toggles',start_ns=None,end_ns=None,merge_buses=False):
        min_time, max_time = self.fsdbfile.min_time(), self.fsdbfile.max_time()
        start = int(start_ns / self.scale_to_ns()) if start_ns is not None else min_time
        end   = int(end_ns   / self.scale_to_ns()) if end_ns   is not None else max_time

        assert end <= max_time, "Interval end must be before FSDB end"

        emb_options = ['toggles','ones','trans']
        emb = {k: (embedding==k) for k in emb_options}
        assert(sum(emb.values())  == 1), f"embedding must be one of {emb_options}"

        self.fsdbfile.unload_vc()
        self.fsdbfile.reset_sig_list()
        self.fsdbfile.load_vc_by_range(start,end) # don't think this works...it just loads whole range

        sig = self.fsdbfile.sig_by_name(sn)
        sr = (sig.left_range(),sig.right_range())
        sw = max(sr) - min(sr) + 1

        vct = sig.create_vct()
        self.fsdbfile.load_vc_by_range(start,end) # don't think this works...it just loads whole range

        # vct.goto_first()
        vct.goto_time(start)
        sigval_dict = {start: vct.value()}
        while vct.goto_next():
            if vct.time() > end: break
            sigval_dict[vct.time()] = vct.value()

        vct.release()
        vct = None
        self.fsdbfile.unload_vc()
        
        sigvals = pd.Series(sigval_dict,name=sn,dtype="string")
        sigvals = sigvals.sort_index()
        sigvals = sigvals.ffill(axis=0)

        sigbits = {}
        if sw > 0:
            for bit in range(0,sw):
                sigbits[sn+f'-bit{bit}'] = sigvals.str[bit]
            sigbits = pd.DataFrame(sigbits)
        else: sigbits = sigvals.to_frame(sn)
        sigbits = sigbits.replace('x','0')  # TODO: not sure how to represent x values...
        sigbits = sigbits.astype(int).astype(bool)

        times = sigbits.index.values
        time_ns = times * self.scale_to_ns()
        windows = (time_ns // window_ns).astype(np.uint64)

        # remove first value b/c this is start time
        if emb['toggles'] or emb['trans']:
            # display(sigbits,windows,time_ns)
            df_emb = sigbits.diff().iloc[1:].astype(bool)
            windows = windows[1:]
        elif emb['ones']:
            # add all window boundaries
            # + \
            # idxs = [i for i in range(window_ns-1,int(time_ns[-1]+window_ns),window_ns)]
            idxs = [i for i in range(0,int(time_ns[-1]+window_ns),window_ns)]
            idxs = [int(i / self.scale_to_ns()) for i in idxs if i not in sigbits.index]
            sigbits = pd.concat([sigbits.astype("boolean"),
                                    pd.DataFrame(pd.NA,index=idxs,columns=sigbits.columns,dtype="boolean")],
                                    axis=0).sort_index().ffill().astype(bool)
            times = sigbits.index.values
            time_ns = times * self.scale_to_ns()
            windows = (time_ns // window_ns).astype(np.uint64)
            dfs = []
            for col in sigbits.columns:
                cur_vals = sigbits[col]
                cur_time = time_ns.copy()
                next_time = np.append(time_ns[1:],np.inf)
                window_end = (windows + 1) * window_ns
                assert((cur_time <= window_end).all())
                mask = (next_time > window_end)
                next_time[mask] = window_end[mask]
                time_diff = next_time - cur_time
                assert((time_diff <= window_ns).all())
                ones = np.zeros(cur_vals.shape)
                # ilocs = np.where(cur_vals)[0]
                # display(ilocs.shape,ones[ilocs].shape,time_diff[ilocs].reshape(ilocs.shape).shape)
                # display(list(zip(cur_time,next_time,window_end,time_diff.astype(int))))
                ones[cur_vals] = time_diff[cur_vals]
                ones = ones / (window_ns/1000)
                dfs += [pd.Series(ones,index=sigbits.index,name=col)]
            df_emb = pd.concat(dfs,axis=1)

        # window data
        grouped = df_emb.groupby(by=windows)
        if emb['trans']:    df_windows = grouped.any()
        else:               df_windows = grouped.sum()
        df_windows = df_windows.astype(np.uint16)  # window values are small
        df_windows.index.name = 'window'

        if merge_buses:
            grps = df_windows.columns.str.split('-bit').str[0]
            grouped = df_windows.groupby(grps,axis=1)
            if emb['trans']: df_windows = grouped.any()
            else: df_windows = grouped.sum()
            df_windows = df_windows.sort_index()
            

        return df_windows


    
    # def interval_embed_signals(self,sigfullnames,window_len,start_ns=None,end_ns=None):
    #     # try: self.loaded_sigs
    #     # except AttributeError:
    #     #     print(f"Signals not loaded, run load_sigs_by_names() first")
    #     #     raise Exception
    #     keys = ['intro','createvc','gotonext','pdseries','window','concat']
    #     times = {k: timedelta(0) for k in keys}
    #     chkpt = datetime.now()
    #     start,end = (int(t / self.scale_to_ns()) if t is not None else None for t in (start_ns,end_ns))
    #     min_time, max_time = self.fsdbfile.min_time(), self.fsdbfile.max_time()
    #     assert end <= max_time, "Interval end must be before FSDB end"
    #     toggles_l = []
    #     ones_l = []
        
    #     self.fsdbfile.unload_vc()
    #     self.fsdbfile.reset_sig_list()
    #     if start or end: self.fsdbfile.load_vc_by_range(start,end) # don't think this works...it just loads whole range
    #     times['intro'] += datetime.now() - chkpt

    #     for i,sn in enumerate(sigfullnames):
    #         chkpt = datetime.now()
    #         sig = self.fsdbfile.sig_by_name(sn)
    #         sr = (sig.left_range(),sig.right_range())
    #         sw = max(sr) - min(sr) + 1
            
    #         vct = sig.create_vct()
    #         if start or end: self.fsdbfile.load_vc_by_range(start,end) # don't think this works...it just loads whole range

    #         times['createvc'] += datetime.now() - chkpt
    #         chkpt = datetime.now()
            
    #         # ret = vct.goto_first()
    #         ret =  vct.goto_time(start) if start else vct.goto_first()
    #         sigval_dict = {vct.time(): vct.value()}
    #         while vct.goto_next():
    #             if end and (vct.time() > end): break
    #             sigval_dict[vct.time()] = vct.value()
    #         # if len(sigval_dict) > 1e5: print(i,len(sigval_dict))

    #         times['gotonext'] += datetime.now() - chkpt
    #         chkpt = datetime.now()

    #         vct.release()
    #         vct = None
    #         self.fsdbfile.unload_vc()
            
    #         sigvals = pd.Series(sigval_dict,name=sig.full_name(),dtype="string")
    #         sigvals = sigvals.sort_index()
    #         sigvals = sigvals.ffill(axis=0)

    #         if i % 100 == 0:
    #             print(f"\tsig #{i}...")
                

    #         sigbits = {}
    #         if sw > 1:
    #             for bit in range(0,sw):
    #                 sigbits[sn+f'-bit{bit}'] = sigvals.str[bit]
    #             sigbits = pd.DataFrame(sigbits)
    #         else: sigbits = sigvals.to_frame(sn)
    #         sigbits = sigbits.replace('x','0')  # TODO: not sure how to represent x values...
    #         sigbits = sigbits.astype(int).astype(bool)

    #         # remove first value b/c this is start time
    #         toggles = sigbits.diff().iloc[1:].astype(np.uint64)
    #         sigbits = sigbits.iloc[1:]

    #         times['pdseries'] += datetime.now() - chkpt
    #         chkpt = datetime.now()

    #         # window data
    #         time_ns = sigbits.index.values * self.scale_to_ns()
    #         windows = (time_ns // window_len).astype(np.uint64)
    #         toggle_windows = toggles.groupby(by=windows,axis=0).sum()
    #         ones_windows = sigbits.groupby(by=windows,axis=0).sum()

    #         toggles_l.append(toggle_windows)
    #         ones_l.append(ones_windows)

    #         times['window'] += datetime.now() - chkpt
    #         chkpt = datetime.now()
            
        
    #     dfs = []
    #     for l in [toggles_l,ones_l]:
    #         df = pd.concat(l,axis=1).sort_index()
    #         df.index = df.index.astype(np.uint64)
    #         df = df.fillna(0).astype(np.uint16)
    #         df.index.name = 'window'
    #         dfs.append(df)
        
    #     times['concat'] += datetime.now() - chkpt

    #     print("\tDone")

    #     for k,t in times.items():
    #         print(k,round(t.total_seconds(),1))

    #     return (df for df in dfs)


