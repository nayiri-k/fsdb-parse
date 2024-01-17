#!/usr/bin/env python3
import os, sys
import argparse
import scipy.sparse
import numpy as np

exec(open("/tools/C/nayiri/power/scripts/variables.py").read())

def main():
    parser = argparse.ArgumentParser(prog="compress_array",
                                     description="Convert toggle array to scipy sparse matrix (CSC) format *.npz file.")
    parser.add_argument('-b', '--bin', type=str, help="Toggles file", required=True)
    parser.add_argument('-c', '--csc', type=str, help="Toggles CSC file", required=True)
    args = parser.parse_args()

    # load matrix from *.bin file & reshape
    bin_files = args.bin.split()
    toggles = None

    for bin_file in bin_files:
        if not os.path.exists(bin_file):
            print(f"ERROR: Toggle bin file does not exist, re-run dump_toggles.py/dt step ({bin_file})")
            return
        array = np.fromfile(bin_file,dtype=np.uint16)
        N = np.uint32(array[1]) << 16 | np.uint32(array[0])
        M = np.uint32(array[3]) << 16 | np.uint32(array[2])
        arr_reshape = array[4:].reshape(-1,M)
        w_toggles = np.transpose(arr_reshape)
        print(f"dim={(N,M)}, max={w_toggles.max()}, ", bin_file)

        if toggles is None: toggles = w_toggles
        else: toggles = np.concatenate((toggles, w_toggles))

        os.remove(bin_file)

    # store as CSC file (much more efficient than CSR)
    sparse_matrix = scipy.sparse.csc_matrix(toggles)
    csc_file = args.csc
    scipy.sparse.save_npz(csc_file,sparse_matrix)
    print(f"Moved toggles file to: {toggles.shape} {csc_file}")

if __name__ == "__main__":
    main()

