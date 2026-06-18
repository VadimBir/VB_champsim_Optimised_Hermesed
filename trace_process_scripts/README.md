# trace_decode

Standalone multi-threaded xz trace decoder. Mirrors ChampSim v14 `XZReader`
(libLZMA streaming, `LZMA_CONCATENATED`) but with much larger buffers and a
1-decoder / N-processor design.

## Build
```
cd /home/cc/champsim_VB/trace_process_scripts && make
```

## Run
```
./trace_decode LLM1024.GPT-125M_496M.champsimtrace.xz
./trace_decode /dev/shm/traces/<file>.xz --threads 16 --limit 100000000
```

Argument may be an absolute path, a filename under `/dev/shm/traces/`, or a
unique substring of a filename in that directory.

The per-record processing hook is a placeholder; see
`// === PHASE-ANALYSIS HOOK GOES HERE ===` in `trace_decode.cc`.
