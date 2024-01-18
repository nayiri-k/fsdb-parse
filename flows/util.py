exec(open("./variables.py").read())


def get_workload_sigvals(w,rtl='RocketConfig',idcode=None):
    # reads in binary format that is written out by ~fsdb-parse/cpp/dump_signal.cpp
    def convert_uint1_to_uint32(vals):
        val = np.uint32(0)
        # for i in range(len(vals)-1,-1,-1):
        for i in range(0,len(vals)):
            if ((vals[i] != 0) and (vals[i] != 1)): return -1
            val = val << 1 | np.uint32(vals[i])
        return val

    def convert_uint8_to_uint32(vals):
        val = np.uint32(0)
        for i in range(len(vals)-1,-1,-1):
            val = val << 8 | np.uint32(vals[i])
        return val
    
    bin_file = get_outfile_path('sigvals',w,rtl=rtl, idcode=idcode)

    array = np.fromfile(bin_file,dtype=numpy.uint8)
    bitwidth = convert_uint8_to_uint32(array[0:4])
    interval = 8+bitwidth
    times = [convert_uint8_to_uint32(array[i:i+8]) for i in range(4,len(array)-8+1,interval)]
    sigvals = [convert_uint1_to_uint32(array[i:i+bitwidth]) for i in range(4+8,len(array),interval)]
    return times, sigvals


def get_inst_module_dict(rtl='RocketConfig'):
    inst_module_dict = {}

    tmh = {}
    with open(get_tmh_path(rtl), 'r') as f:
        tmh = json.load(f)

    root = tmh['instance_name']
    parent_children = [(root, tmh['instances'])]
    inst_module_dict[tmh['instance_name']] = tmh['module_name']

    while parent_children:
        parent, children = parent_children.pop()
        for child_dict in children:
            child = child_dict['instance_name']
            child_path = parent+'/'+child
            inst_module_dict[child_path] = child_dict['module_name']
            parent_children.append((child_path, child_dict['instances']))
    return inst_module_dict




def get_module_insts_dict(rtl='RocketConfig',root_name='chiptop'):
    module_insts_dict = {}
    inst_module_dict = get_inst_module_dict(rtl)
    for inst,module in inst_module_dict.items():
        if module not in module_insts_dict: module_insts_dict[module] = []
        module_insts_dict[module].append(inst)
    module_insts_dict['ChipTop'] = [root_name]
    return module_insts_dict



def dump_idcodes(rtl='',module='ChipTop',inst='chiptop'):
    top = 'chiptop' # needs to be lowercase

    module_insts_dict = get_module_insts_dict(rtl)
    module_insts_dict['ChipTop'] = ['ChipTop'] # instead of 'chiptop'
    instance = module_insts_dict[module][0]
    instance_paths = [inst for insts in module_insts_dict.values() for inst in insts if inst.startswith(instance)]
    instance_names = {inst.split('/')[-1] for inst in instance_paths}

    signal_id_dict = {}
    id_width_dict = {}
    id_signal_dict = {}
    inst_hier = []
    inst_hier_path = ""
    inst_hier_path_chiptop = ""
    with open(get_outfile_path('headers',rtl=rtl), 'r') as f:
        for line in f.readlines():
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
                if inst_hier_path_chiptop not in instance_paths: continue
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
                    id_width_dict[idcode] = width
                else:
                    id_width_dict[idcode] = 0

    args_dict = {
        'rtl': rtl,
        'module': module,
    }
    
    id_signals = sorted(id_signal_dict.items()) #[(i,s) for i,s in sorted(id_signal_dict.items())]

    ids = [id for id,_ in id_signals]
    sigwidths = [id_width_dict[id] for id in ids]
    num_idcodes = len(ids)
    num_bits = sum([i+1 for i in sigwidths])*num_idcodes

    signames_idcodes_fpath = get_outfile_path('idcodes','signames',**args_dict)
    idcodes_fpath = get_outfile_path('idcodes',**args_dict)
    sigwidths_fpath = get_outfile_path('sigwidths',rtl=rtl,module=module)
    print('total signals:', len(id_signals))
    print(f'[num_signals] + [signal fsdb idcodes] --> {idcodes_fpath}')
    print(f'[num_signals] + [(signal fsdb idcode, signal name(s)), ...] --> {signames_idcodes_fpath}')

    
    with open(idcodes_fpath,'w') as fi:
        with open(sigwidths_fpath,'w') as fs:
            fi.write(f"{num_idcodes} ")
            # fs.write(f"{num_bits} ")
            for id,sigwidth in zip(ids,sigwidths):
                fi.write(f"{id} ")
                fs.write(f"{sigwidth} ")
    
    with open(signames_idcodes_fpath,'w') as f:
        f.write(f"{len(id_signals)}\n")
        for id,sigs in id_signals:
            f.write(f"{id} {','.join(sigs)}\n")

    return

