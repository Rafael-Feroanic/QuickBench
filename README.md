# QuickBench

Fast, configurable, open-source benchmarking suite written in C.

> **Alpha stage:** APIs, benchmark semantics, output formats, and workload
> implementations are still subject to change.

## Goals

- Low measurement overhead
- Explicit, user-controlled workloads
- Reproducible runs
- Safe failure handling
- Human, JSON, and CSV output
- Linux-first implementation with a portable core
- Architecture-specific optimizations isolated from the benchmark engine

## Build

```sh
make
make test
```

Debug/sanitizer build:

```sh
make SANITIZE=1
```

## First alpha commands

```sh
./quickbench --help
./quickbench --version
./quickbench --list
./quickbench cpu --threads 4 --duration 10
./quickbench cpu --threads 4 --duration 10 --load 50
```

## Project layout

```text
include/quickbench.h   Public API
src/main.c             CLI and alpha runner
tests/smoke.sh         Basic executable tests
docs/ARCHITECTURE.md   Design notes
```

## License

MIT. See `LICENSE`.
