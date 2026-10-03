#!/bin/sh
set -eu

./quickbench --version
./quickbench --list
./quickbench --help >/dev/null
./quickbench cpu --threads 1 --duration 0.1 --load 100 >/dev/null
./quickbench cpu --threads 2 --duration 0.1 --load 50 >/dev/null

echo "QuickBench smoke tests: PASS"
