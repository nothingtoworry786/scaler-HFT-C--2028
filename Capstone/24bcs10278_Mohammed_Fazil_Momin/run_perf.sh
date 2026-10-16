#!/bin/sh
# Run on a Linux machine with perf installed. Collects everything the report needs into results/.
set -e
cd "$(dirname "$0")"

mkdir -p results
make bench test_a test_b test_c

echo "== unit tests ==" | tee results/tests.txt
./test_a | tee -a results/tests.txt
./test_b | tee -a results/tests.txt
./test_c | tee -a results/tests.txt

echo "== verify ==" | tee results/verify.txt
./bench --verify | tee -a results/verify.txt

EVENTS=task-clock,cycles,instructions,branches,branch-misses,cache-misses,L1-dcache-load-misses,dTLB-load-misses,page-faults

for IMPL in A B C; do
    echo "== bench $IMPL ==" | tee results/bench_$IMPL.txt
    ./bench --impl $IMPL | tee -a results/bench_$IMPL.txt

    echo "== perf stat $IMPL ==" | tee results/perf_stat_$IMPL.txt
    perf stat -r 5 -e $EVENTS ./bench --impl $IMPL --runs 1 2>&1 | tee -a results/perf_stat_$IMPL.txt

    echo "== perf record $IMPL ==" | tee results/perf_report_$IMPL.txt
    perf record -g -o results/perf_$IMPL.data ./bench --impl $IMPL --runs 1 > /dev/null 2>&1
    perf report -i results/perf_$IMPL.data --stdio --no-children 2>/dev/null | head -80 | tee -a results/perf_report_$IMPL.txt
done

echo
echo "Done. Outputs are in results/. Copy perf_stat_*.txt into REPORT.md."
