#define _GNU_SOURCE
#define _POSIX_C_SOURCE 200809L

#include "quickbench_topology.h"

#include <cpuid.h>
#include <errno.h>
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define QB_SYSFS_CPU_PATH "/sys/devices/system/cpu"

static int read_uint_file(const char *path, unsigned *value)
{
    FILE *file;
    unsigned tmp;

    file = fopen(path, "r");
    if (file == NULL) {
        return -1;
    }

    if (fscanf(file, "%u", &tmp) != 1) {
        fclose(file);
        return -1;
    }

    fclose(file);
    *value = tmp;

    return 0;
}

static int read_cpu_topology(unsigned cpu,
                             unsigned *package_id,
                             unsigned *core_id)
{
    char path[256];

    (void)snprintf(
        path,
        sizeof(path),
        QB_SYSFS_CPU_PATH "/cpu%u/topology/physical_package_id",
        cpu
    );

    if (read_uint_file(path, package_id) != 0) {
        return -1;
    }

    (void)snprintf(
        path,
        sizeof(path),
        QB_SYSFS_CPU_PATH "/cpu%u/topology/core_id",
        cpu
    );

    if (read_uint_file(path, core_id) != 0) {
        return -1;
    }

    return 0;
}

#if defined(__x86_64__) || defined(__i386__)

static enum qb_cpu_type detect_x86_core_type(void)
{
    unsigned int max_leaf;
    unsigned int eax;
    unsigned int ebx;
    unsigned int ecx;
    unsigned int edx;
    unsigned int core_type;

    max_leaf = __get_cpuid_max(0, NULL);

    if (max_leaf < 0x1AU) {
        return QB_CPU_TYPE_UNKNOWN;
    }

    if (!__get_cpuid_count(0x1AU, 0, &eax, &ebx, &ecx, &edx)) {
        return QB_CPU_TYPE_UNKNOWN;
    }

    core_type = (eax >> 24U) & 0xFFU;

    switch (core_type) {
    case 0x40U:
        return QB_CPU_TYPE_P;

    case 0x20U:
        return QB_CPU_TYPE_E;

    default:
        return QB_CPU_TYPE_UNKNOWN;
    }
}

static int detect_cpu_type(unsigned cpu, enum qb_cpu_type *type)
{
    cpu_set_t original;
    cpu_set_t target;

    if (sched_getaffinity(0, sizeof(original), &original) != 0) {
        return errno;
    }

    CPU_ZERO(&target);
    CPU_SET(cpu, &target);

    if (sched_setaffinity(0, sizeof(target), &target) != 0) {
        return errno;
    }

    *type = detect_x86_core_type();

    if (sched_setaffinity(0, sizeof(original), &original) != 0) {
        return errno;
    }

    return 0;
}

#else

static int detect_cpu_type(unsigned cpu, enum qb_cpu_type *type)
{
    (void)cpu;

    *type = QB_CPU_TYPE_UNKNOWN;

    return 0;
}

#endif

int qb_topology_detect(struct qb_topology *topology)
{
    cpu_set_t allowed;
    long configured_cpus;
    size_t count = 0;
    size_t index = 0;
    struct qb_cpu_info *cpus;

    if (topology == NULL) {
        return EINVAL;
    }

    topology->cpus = NULL;
    topology->cpu_count = 0;

    if (sched_getaffinity(0, sizeof(allowed), &allowed) != 0) {
        return errno;
    }

    configured_cpus = sysconf(_SC_NPROCESSORS_CONF);

    if (configured_cpus <= 0) {
        return EIO;
    }

    for (long cpu = 0; cpu < configured_cpus; ++cpu) {
        if (cpu < 0L || cpu >= (long)CPU_SETSIZE) {
            continue;
        }

        if (CPU_ISSET((unsigned)cpu, &allowed) == 0) {
            continue;
        }

        ++count;
    }

    if (count == 0) {
        return EIO;
    }

    cpus = calloc(count, sizeof(*cpus));

    if (cpus == NULL) {
        return ENOMEM;
    }

    for (long cpu = 0; cpu < configured_cpus; ++cpu) {
        unsigned package_id;
        unsigned core_id;
        enum qb_cpu_type type = QB_CPU_TYPE_UNKNOWN;
        int rc;

        if (cpu < 0L || cpu >= (long)CPU_SETSIZE) {
            continue;
        }

        if (CPU_ISSET((unsigned)cpu, &allowed) == 0) {
            continue;
        }

        rc = read_cpu_topology(
            (unsigned)cpu,
            &package_id,
            &core_id
        );

        if (rc != 0) {
            free(cpus);
            return EIO;
        }

        rc = detect_cpu_type((unsigned)cpu, &type);

        if (rc != 0) {
            free(cpus);
            return rc;
        }

        cpus[index].cpu = (unsigned)cpu;
        cpus[index].package_id = package_id;
        cpus[index].core_id = core_id;
        cpus[index].type = type;

        ++index;
    }

    topology->cpus = cpus;
    topology->cpu_count = index;

    return 0;
}

void qb_topology_free(struct qb_topology *topology)
{
    if (topology == NULL) {
        return;
    }

    free(topology->cpus);

    topology->cpus = NULL;
    topology->cpu_count = 0;
}

const char *qb_cpu_type_name(enum qb_cpu_type type)
{
    switch (type) {
    case QB_CPU_TYPE_P:
        return "P";

    case QB_CPU_TYPE_E:
        return "E";

    case QB_CPU_TYPE_UNKNOWN:
    default:
        return "unknown";
    }
}

void qb_topology_print(const struct qb_topology *topology)
{
    if (topology == NULL) {
        return;
    }

    printf("CPU topology\n");
    printf("  logical CPUs : %zu\n", topology->cpu_count);
    printf("\n");

    for (size_t i = 0; i < topology->cpu_count; ++i) {
        const struct qb_cpu_info *cpu = &topology->cpus[i];

        printf(
            "  CPU %-3u package=%u core=%u type=%s\n",
            cpu->cpu,
            cpu->package_id,
            cpu->core_id,
            qb_cpu_type_name(cpu->type)
        );
    }
}
