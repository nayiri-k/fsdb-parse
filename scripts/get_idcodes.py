#!/usr/bin/env python3
import os, sys, json
import argparse

exec(open(f"/tools/C/nayiri/power/scripts/util.py").read())

top = 'chiptop' # needs to be lowercase

def main():
    parser = argparse.ArgumentParser(prog="get_idcodes",
                                     description="Write power event signal names in FSDB waveform to file.")
    parser.add_argument('--rtl', type=str, help="Name of  Chipyard Config", default='RocketConfig')
    parser.add_argument('--module', type=str, help="Only dump signals for one instance of this module", default='ChipTop')
    parser.add_argument('--inst', type=str, help="Only dump signals for this instance [NOT IMPLEMENTED]", default='chiptop')
    parser.add_argument('--power_events', action="store_true", help="Only dump signals corresponding to power events")
    args = parser.parse_args()


    module_insts_dict = get_module_insts_dict(args.rtl)
    module_insts_dict['ChipTop'] = ['ChipTop'] # instead of 'chiptop'
    instance = module_insts_dict[args.module][0]
    instance_paths = [inst for insts in module_insts_dict.values() for inst in insts if inst.startswith(instance)]
    instance_names = {inst.split('/')[-1] for inst in instance_paths}

    signal_id_dict = {}
    id_width_dict = {}
    id_signal_dict = {}
    inst_hier = []
    inst_hier_path = ""
    inst_hier_path_chiptop = ""
    with open(get_outfile_path('headers',rtl=args.rtl), 'r') as f:
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
                if args.power_events:
                    if not signal_name.startswith("power_event_"): continue
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

    # signals = set(id_signal_dict.values())

    args_dict = {
        'rtl': args.rtl,
        'module': args.module,
        'power_events': args.power_events
    }

    
    # signal_id_dict = {s:i for s,i in signal_id_dict.items() if s in signals}
    id_signals = sorted(id_signal_dict.items()) #[(i,s) for i,s in sorted(id_signal_dict.items())]

    ids = [id for id,_ in id_signals]
    sigwidths = [id_width_dict[id] for id in ids]
    num_idcodes = len(ids)
    num_bits = sum([i+1 for i in sigwidths])*num_idcodes

    print('\ntotal'.ljust(10), len(id_signals), '-->',
          get_outfile_path('idcodes','signames',**args_dict))

    with open(get_outfile_path('idcodes',**args_dict),'w') as fi:
        with open(get_outfile_path('sigwidths',rtl=args.rtl,module=args.module),'w') as fs:
            fi.write(f"{num_idcodes} ")
            # fs.write(f"{num_bits} ")
            for id,sigwidth in zip(ids,sigwidths):
                fi.write(f"{id} ")
                fs.write(f"{sigwidth} ")

    with open(get_outfile_path('idcodes','signames',**args_dict),'w') as f:
        f.write(f"{len(id_signals)}\n")
        for id,sigs in id_signals:
            f.write(f"{id} {','.join(sigs)}\n")

if __name__ == "__main__":
    main()


        