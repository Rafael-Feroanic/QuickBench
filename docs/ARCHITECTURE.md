# QuickBench architecture

## Alpha 1

The first alpha intentionally focuses on a small, auditable execution engine.

### Core rules

1. Benchmark code must not invoke a shell or external executable.
2. Measurement timing uses a monotonic clock.
3. Worker creation and synchronization are outside the timed workload where possible.
4. Every workload reports a well-defined unit.
5. Results must be machine-readable without parsing human output.
6. Architecture-specific code belongs behind an explicit backend interface.
7. Invalid user input fails before starting a benchmark.
8. Interrupts stop workers cleanly.

## Planned modules

```text
core/
  runner
  scheduler
  timing
  statistics

workloads/
  cpu
  integer
  float
  simd
  memory
  cache
  mixed

platform/
  linux
    cpuinfo
    sysfs
    affinity
    thermal
    frequency

output/
  human
  json
  csv
```

## Benchmark integrity

A benchmark result is only useful if the program can explain exactly what was
measured. Future results will therefore include workload version, compiler
information, architecture, thread placement, duration, warm-up, repetition
count, and relevant system state.

The current alpha CPU kernel is explicitly provisional and should not be used
for cross-machine performance rankings.
