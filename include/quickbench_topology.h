#ifndef QUICKBENCH_TOPOLOGY_H
#define QUICKBENCH_TOPOLOGY_H

#include <stddef.h>

enum qb_cpu_type {
    QB_CPU_TYPE_UNKNOWN = 0,
    QB_CPU_TYPE_P,
    QB_CPU_TYPE_E
};

struct qb_cpu_info {
    unsigned cpu;
    unsigned package_id;
    unsigned core_id;
    enum qb_cpu_type type;
};

struct qb_topology {
    struct qb_cpu_info *cpus;
    size_t cpu_count;
};

int qb_topology_detect(struct qb_topology *topology);
void qb_topology_free(struct qb_topology *topology);

const char *qb_cpu_type_name(enum qb_cpu_type type);

void qb_topology_print(const struct qb_topology *topology);

#endif
