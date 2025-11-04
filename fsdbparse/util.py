from pathlib import Path
import yaml
import sklearn
import pandas as pd


def to_ns(timeval,unit=None):
    timeval = timeval.replace('.0','')
    t = ''.join([c for c in timeval if c.isnumeric()])
    if unit is None: unit = ''.join([c for c in timeval if not c.isnumeric()])
    conv = dict(s=1e9,ms=1e6,us=1e3,ns=1,ps=1e-3,fs=1e-6)
    return int(t) * conv[unit]


def dump_vector(fp, arr):
    if type(fp) == str: fp = Path(fp)
    fp.parent.mkdir(exist_ok=True,parents=True)
    if fp.suffix in {'.yml','.yaml'}:
        Util.dump_yaml(fp,arr)
    else:
        with fp.open('w') as f:
            f.write(" ".join([str(i) for i in arr]))


def load_vector(fp,dtype=int):
    if type(fp) == str: fp = Path(fp)
    if fp.suffix in {'.yml','.yaml'}:
        return Util.load_yaml(fp)
    else:
        with fp.open('r') as f:
            return [*map(dtype,f.readlines()[0].split())]

