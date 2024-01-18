dirs = os.path.abspath(os.getcwd()).split('/')
root_dir = '/'.join(dirs[:dirs.index('fsdb-parse')+1])

try:
    chipyard_dir
except NameError:
    print("ERROR: Variable chipyard_dir must be set to root of your chipyard install")
    exit(1)


analysis_output_dir = f"{root_dir}/out"
vlsi_dir = f"{chipyard_dir}/vlsi"
build_dir = f"{vlsi_dir}/build"
output_dir = f"{vlsi_dir}/output"




riscv_benchmarks = ['dhrystone', 'median', 'mm', 'mt-matmul', 'mt-vvadd', 'multiply', 'qsort', 'rsort', 'spmv', 'towers', 'vvadd']
workload_names = riscv_benchmarks
workloads = {w: {} for w in workload_names}
riscv_tests_path = f"{chipyard_dir}/.conda-env/riscv-tools/riscv64-unknown-elf/share/riscv-tests/benchmarks"
for w in riscv_benchmarks:
    workloads[w]['binary_path'] = f"{riscv_tests_path}/{w}.riscv"


fpath_kwargs = {
    'rtl': 'RocketConfig',
    'start_end': None,
    'power_events': False,
    'compressed': False, 
    'sampled': False,
    'idcode': None
}
def get_outfile_path(ftype,name='',**kwargs):
    merged_kwargs = {**fpath_kwargs, **kwargs}

    rtl = merged_kwargs['rtl']
    start_end = merged_kwargs['start_end']
    compressed = merged_kwargs['compressed']
    idcode = merged_kwargs['idcode']

    types = {'headers','idcodes','sigwidths',
            'toggles', 'ones', 'endtime',
            'sigvals', 'model_config', 'spike'}
    assert(ftype in types), f"Type ({ftype}) must be one of: {types}"
    create_dir = True
    ext = 'npz' if compressed else 'bin'

    if start_end is not None:
        s,e = start_end
        if s != 0: start_time = s
        if e is not None: end_time = e
    if ftype in {'toggles', 'ones'}:
        if ftype in {'toggles', 'ones'}:
            outdir = f"{analysis_output_dir}/fsdb/{ftype}"
        else:
            ext = 'txt'
        fpath = f"{outdir}/{rtl}/{name}.{ext}"
    elif ftype == 'headers':
        fpath = f"{analysis_output_dir}/fsdb/{ftype}/{rtl}.txt"
    elif ftype in {'idcodes','sigwidths'}:
        name = 'signames-' if name == 'signames' else ''
        fpath = f"{analysis_output_dir}/fsdb/{ftype}/{name}{rtl}.txt"
    elif ftype == 'endtime':
        fpath = f"{analysis_output_dir}/fsdb/endtime/{rtl}/{name}.txt"
    elif ftype == 'sim_out':
        fpath = f"{output_dir}/chipyard.harness.TestHarness.{rtl}/{name}.out"
        create_dir = False
    elif ftype == 'spike':
        fpath = f"{analysis_output_dir}/spike/{rtl}/{name}.spike.log"
    elif ftype == f'sigvals':
        extra = '' if idcode is None else f'-idcode_{idcode}' 
        fpath = f"{analysis_output_dir}/fsdb/{rtl}/{name}{extra}.bin"
    if create_dir:
        os.makedirs('/'.join(fpath.split('/')[:-1]), exist_ok=True)
    return fpath


def get_generated_src_path(rtl):
    return f"{vlsi_dir}/generated-src/chipyard.harness.TestHarness.{rtl}"


def get_tmh_path(rtl):
    return f"{get_generated_src_path(rtl)}/top_module_hierarchy.json"


def get_waveform_path(workload_name, rtl='RocketConfig', power_level='rtl'):
    waveform_outdir = f"{output_dir}/chipyard.harness.TestHarness.{rtl}"
    wp = f"{waveform_outdir}/{workload_name}.fsdb"
    return wp
