#ifndef QUICKBENCH_H
#define QUICKBENCH_H

#include <stdint.h>

#define QB_VERSION "0.1.0-alpha.1"

typedef enum {
    QB_WORKLOAD_CPU = 0
} qb_workload_t;

typedef struct {
    qb_workload_t workload;
    uint32_t threads;
    double duration_seconds;
    double load_percent;
    double warmup_seconds;
} qb_config_t;

typedef struct {
    uint64_t operations;
    double elapsed_seconds;
    double operations_per_second;
} qb_result_t;

int qb_run(const qb_config_t *config, qb_result_t *result);

#endif
