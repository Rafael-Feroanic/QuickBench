# Contributing to QuickBench

QuickBench is in alpha. Prefer small, reviewable changes.

## Before submitting code

```sh
make clean
make
make test
make SANITIZE=1
```

Do not add shell commands to the benchmark execution path.

Benchmark changes should document:
- workload definition
- unit of work
- timing boundaries
- compiler optimization considerations
- architecture assumptions
- expected failure modes
