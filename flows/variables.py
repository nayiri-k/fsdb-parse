import os
import yaml
import json
from textwrap import dedent
# import numpy as np
# import matplotlib.pyplot as plt



# ASSIGN THESE
root_dir = "/tools/C/nayiri/fsdb-parse"
chipyard_dir = f"/tools/scratch/nayiri/power/chipyard-intech16-sep23"






analysis_output_dir = f"{root_dir}/out"
vlsi_dir = f"{chipyard_dir}/vlsi"

fpath_kwargs = {
    'rtl': 'RocketConfig',
    'module': 'ChipTop',
    'start_end': None,
    'power_events': False,
    'compressed': False, 
    'sampled': False,
    'num_toggles': N_CYCLES,
    'idcode': None
}

def get_outfile_path(ftype,name='',**kwargs):
    merged_kwargs = {**fpath_kwargs, **kwargs}

    rtl = merged_kwargs['rtl']
    module = merged_kwargs['module']
    start_end = merged_kwargs['start_end']
    power_events = merged_kwargs['power_events']
    compressed = merged_kwargs['compressed']
    sampled = merged_kwargs['sampled']
    num_toggles = merged_kwargs['num_toggles']
    idcode = merged_kwargs['idcode']

    types = {'headers','idcodes','sigwidths',
            'toggles', 'ones', 'endtime',
            'sigtrans',
            'sigvals','sigval',
            'sim_out','sim_opm','sim_opm_tmp',
            'proxy_signals',
            'model_config', 'power_events_yaml_config',
            'sampled_intervals',
            'hammer_joules_yaml_config', 'spike', 'testdriver'}
    assert(ftype in types), f"Type ({ftype}) must be one of: {types}"
    create_dir = True
    start_time = ''
    end_time = ''

    extra = "-events" if power_events else ""
    if type(power_events) == str: extra = "-events_"+power_events
    ext = 'npz' if compressed else 'bin'
    
    if sampled: name += '.sampled'
    new_name = name
    if num_toggles != 100: new_name += f".toggles-{num_toggles}"

    if power_events: module='ChipTop'
    if start_end is not None:
        s,e = start_end
        if s != 0: start_time = s
        if e is not None: end_time = e
    if ftype in {'toggles', 'proxy_signals', 'ones',
                    'sigtrans'}:
        if ftype in {'toggles', 'ones',
                        'sigtrans'}:
            outdir = f"{analysis_output_dir}/fsdb/{ftype}"
        elif ftype in {'proxy_signals'}:
            outdir = f"{analysis_output_dir}/{ftype}"
            new_name=rtl
        if ftype in {'toggles', 'ones',
                        'sigtrans'}:
            if start_time != '':
                if not isinstance(start_time, str): start_time = f"{start_time}ns"
                extra += f"-start_{start_time}"
            if end_time != '':
                if not isinstance(end_time, str): end_time = f"{end_time}ns"
                extra += f"-end_{end_time}"
        else:
            ext = 'txt'
        m = ''
        if ftype in {'toggles', 'ones',
                    'sigtrans'}:
            m = '' if module == 'ChipTop' else f"-module_{module}"
        fpath = f"{outdir}/{rtl}/{new_name}{m}{extra}.{ext}"
    elif ftype == 'headers':
        fpath = f"{analysis_output_dir}/fsdb/{ftype}/{rtl}.txt"
    elif ftype in {'idcodes','sigwidths'}:
        m = '' if module == 'ChipTop' else f"-module_{module}"
        new_name = 'signames-' if name == 'signames' else ''
        fpath = f"{analysis_output_dir}/fsdb/{ftype}/{new_name}{rtl}{m}{extra}.txt"
    elif ftype == 'endtime':
        fpath = f"{analysis_output_dir}/fsdb/endtime/{rtl}/{new_name}.txt"
    elif ftype == 'sim_out':
        fpath = f"{output_dir}/chipyard.harness.TestHarness.{rtl}/{new_name}.out"
        create_dir = False
    elif ftype == 'sim_opm':
        fpath = f"{output_dir}/chipyard.harness.TestHarness.{rtl}/{new_name}.opm"
        create_dir = False
    elif ftype == 'sim_opm_tmp':
        fpath = f"{output_dir}/chipyard.harness.TestHarness.{rtl}/tmp.opm"
        create_dir = False
    elif ftype == 'sampled_intervals':
        fpath = f"{analysis_output_dir}/sampled_intervals/{rtl}.yml"
    elif ftype == 'hammer_joules_yaml_config':
        extra = '-'+new_name if new_name != '' else ''
        fpath = f"{analysis_output_dir}/yaml_configs/hammer-joules/{rtl}{extra}.yml"
    elif ftype == 'model_config':
        m = f"-{module}" if module != 'ChipTop' else ''
        model_name=f"{new_name}-{rtl}{m}"#-{now}"
        fpath = f"{analysis_output_dir}/models/{model_name}/model.yml"
    elif ftype == 'power_events_yaml_config':
        fpath = f"{analysis_output_dir}/opm/{rtl}/power_events.yml"
    elif ftype == 'spike':
        fpath = f"{analysis_output_dir}/spike/{rtl}/{name}.spike.log"
    elif ftype == 'testdriver':
        fpath = f"{analysis_output_dir}/testdriver/{rtl}/TestDriver-{name}.v"
    elif ftype == f'sigvals':
        if idcode is not None:
            extra = f'-idcode_{idcode}'
        fpath = f"{analysis_output_dir}/fsdb/{rtl}/{name}{extra}.bin"
    if create_dir:
        os.makedirs('/'.join(fpath.split('/')[:-1]), exist_ok=True)
    return fpath



def get_generated_src_path(rtl):
    return f"{vlsi_dir}/generated-src/chipyard.harness.TestHarness.{rtl}"


def get_tmh_path(rtl):
    return f"{get_generated_src_path(rtl)}/top_module_hierarchy.json"


