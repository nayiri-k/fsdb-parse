# FSDB Parsing Library

Custom C scripts to parse through large FSDB waveform files quickly.
Build on top of Verdi FsdbReader library.
Setup only works for BWRC.

## Quickstart

Add necessary libraries to path and compile all binaries.
```
export LD_LIBRARY_PATH=/tools/synopsys/verdi/P-2019.06-SP2-12/share/FsdbReader/linux64:${LD_LIBRARY_PATH}
export LD_LIBRARY_PATH=/opt/rh/devtoolset-8/root/usr/lib64:${LD_LIBRARY_PATH}
make
```

This generates executables in the root directory.

First, we need to print out the FSDB waveform "header" information, which lists all signal information in the design.

```
./print_header <waveform.fsdb>  > header.txt
```

Next, we parse these headers to extract signal idcodes.

```
./scripts/get_idcodes.py <args>
```

From here, we can do a number of things. 

```
// get simulation end time (in ns)
./get_sim_endtime waveform.fsdb end_time.txt

// dump one signal's (time, value changes) to a file
./dump_signal -h

// dump each signal's toggle activity per window of time (creates a matrix)
./dump_toggles -h

// dump each signal's ratio spent with value=1 per window of time (creates a matrix)
./dump_ones -h
```

