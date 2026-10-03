#define _POSIX_C_SOURCE 200809L

#include "quickbench.h"

#include <errno.h>
#include <math.h>
#include <pthread.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

static volatile sig_atomic_t g_stop = 0;
static volatile double g_sink = 0.0;

typedef struct {
    double duration;
    double load;
    uint64_t operations;
} worker_args_t;

static void handle_signal(int signo)
{
    (void)signo;
    g_stop = 1;
}

static double now_seconds(void)
{
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC_RAW, &ts) != 0) {
        return -1.0;
    }
    return (double)ts.tv_sec + ((double)ts.tv_nsec / 1000000000.0);
}

/*
 * A deliberately simple alpha CPU workload.
 *
 * Important: this is NOT yet the final calibrated benchmark kernel.
 * Alpha 1 establishes the runner, timing, threading, load control, and
 * result plumbing before architecture-specific kernels are added.
 */
static uint64_t run_cpu_slice(double seconds)
{
    const double end = now_seconds() + seconds;
    double x = 1.000001;
    uint64_t ops = 0;

    while (!g_stop && now_seconds() < end) {
        for (unsigned i = 0; i < 256U; ++i) {
            x = x * 1.0000001 + 0.0000001;
            x = x * 0.9999999 + 0.0000002;
            ops += 2U;
        }
    }

    g_sink = x;
    return ops;
}

static void *worker_main(void *opaque)
{
    worker_args_t *args = opaque;
    const double active = 0.010;
    const double sleep_time = active * ((100.0 / args->load) - 1.0);
    const double deadline = now_seconds() + args->duration;
    uint64_t total = 0;

    while (!g_stop && now_seconds() < deadline) {
        double slice = active;
        const double remaining = deadline - now_seconds();

        if (remaining < slice) {
            slice = remaining;
        }

        if (slice > 0.0) {
            total += run_cpu_slice(slice);
        }

        if (args->load < 100.0 && sleep_time > 0.0) {
            struct timespec req;
            req.tv_sec = (time_t)sleep_time;
            req.tv_nsec = (long)((sleep_time - (double)req.tv_sec) * 1000000000.0);
            (void)nanosleep(&req, NULL);
        }
    }

    args->operations = total;
    return NULL;
}

static void usage(const char *program)
{
    printf(
        "QuickBench %s\n\n"
        "Usage:\n"
        "  %s cpu [options]\n"
        "  %s --help\n"
        "  %s --version\n"
        "  %s --list\n\n"
        "CPU options:\n"
        "  --threads N       Worker threads (default: online CPUs)\n"
        "  --duration SEC    Benchmark duration (default: 10)\n"
        "  --warmup SEC      Warm-up duration (default: 0)\n"
        "  --load PERCENT    Target duty cycle, 1-100 (default: 100)\n",
        QB_VERSION, program, program, program, program
    );
}

static int parse_u32(const char *s, uint32_t *out)
{
    char *end = NULL;
    unsigned long value;

    errno = 0;
    value = strtoul(s, &end, 10);
    if (errno != 0 || end == s || *end != '\0' || value == 0UL || value > UINT32_MAX) {
        return -1;
    }

    *out = (uint32_t)value;
    return 0;
}

static int parse_double(const char *s, double *out, double min, double max)
{
    char *end = NULL;
    double value;

    errno = 0;
    value = strtod(s, &end);
    if (errno != 0 || end == s || *end != '\0' || !isfinite(value) ||
        value < min || value > max) {
        return -1;
    }

    *out = value;
    return 0;
}

int qb_run(const qb_config_t *config, qb_result_t *result)
{
    pthread_t *threads = NULL;
    worker_args_t *args = NULL;
    uint32_t i;
    double start;
    uint64_t operations = 0;

    if (config == NULL || result == NULL || config->threads == 0U ||
        config->duration_seconds <= 0.0 || config->load_percent <= 0.0 ||
        config->load_percent > 100.0 || config->warmup_seconds < 0.0) {
        return EINVAL;
    }

    if (config->warmup_seconds > 0.0) {
        worker_args_t warmup = {
            .duration = config->warmup_seconds,
            .load = config->load_percent,
            .operations = 0
        };
        (void)run_cpu_slice(warmup.duration);
    }

    threads = calloc(config->threads, sizeof(*threads));
    args = calloc(config->threads, sizeof(*args));
    if (threads == NULL || args == NULL) {
        free(threads);
        free(args);
        return ENOMEM;
    }

    g_stop = 0;
    start = now_seconds();
    if (start < 0.0) {
        free(threads);
        free(args);
        return EIO;
    }

    for (i = 0; i < config->threads; ++i) {
        args[i].duration = config->duration_seconds;
        args[i].load = config->load_percent;
        args[i].operations = 0;

        int rc = pthread_create(&threads[i], NULL, worker_main, &args[i]);
        if (rc != 0) {
            g_stop = 1;
            for (uint32_t j = 0; j < i; ++j) {
                (void)pthread_join(threads[j], NULL);
            }
            free(threads);
            free(args);
            return rc;
        }
    }

    for (i = 0; i < config->threads; ++i) {
        (void)pthread_join(threads[i], NULL);
        operations += args[i].operations;
    }

    result->elapsed_seconds = now_seconds() - start;
    result->operations = operations;
    result->operations_per_second =
        result->elapsed_seconds > 0.0
            ? (double)operations / result->elapsed_seconds
            : 0.0;

    free(threads);
    free(args);
    return 0;
}

int main(int argc, char **argv)
{
    qb_config_t config = {
        .workload = QB_WORKLOAD_CPU,
        .threads = 0U,
        .duration_seconds = 10.0,
        .load_percent = 100.0,
        .warmup_seconds = 0.0
    };
    qb_result_t result;
    long cpus;
    int i;
    int rc;

    if (signal(SIGINT, handle_signal) == SIG_ERR ||
        signal(SIGTERM, handle_signal) == SIG_ERR) {
        fprintf(stderr, "quickbench: failed to install signal handlers\n");
        return EXIT_FAILURE;
    }

    if (argc < 2) {
        usage(argv[0]);
        return EXIT_FAILURE;
    }

    if (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0) {
        usage(argv[0]);
        return EXIT_SUCCESS;
    }

    if (strcmp(argv[1], "--version") == 0 || strcmp(argv[1], "-V") == 0) {
        puts(QB_VERSION);
        return EXIT_SUCCESS;
    }

    if (strcmp(argv[1], "--list") == 0) {
        puts("cpu");
        return EXIT_SUCCESS;
    }

    if (strcmp(argv[1], "cpu") != 0) {
        fprintf(stderr, "quickbench: unknown benchmark '%s'\n", argv[1]);
        return EXIT_FAILURE;
    }

    cpus = sysconf(_SC_NPROCESSORS_ONLN);
    config.threads = cpus > 0 ? (uint32_t)cpus : 1U;

    for (i = 2; i < argc; ++i) {
        if (strcmp(argv[i], "--threads") == 0 && i + 1 < argc) {
            if (parse_u32(argv[++i], &config.threads) != 0) {
                fprintf(stderr, "quickbench: invalid --threads value\n");
                return EXIT_FAILURE;
            }
        } else if (strcmp(argv[i], "--duration") == 0 && i + 1 < argc) {
            if (parse_double(argv[++i], &config.duration_seconds, 0.001, 86400.0) != 0) {
                fprintf(stderr, "quickbench: invalid --duration value\n");
                return EXIT_FAILURE;
            }
        } else if (strcmp(argv[i], "--warmup") == 0 && i + 1 < argc) {
            if (parse_double(argv[++i], &config.warmup_seconds, 0.0, 86400.0) != 0) {
                fprintf(stderr, "quickbench: invalid --warmup value\n");
                return EXIT_FAILURE;
            }
        } else if (strcmp(argv[i], "--load") == 0 && i + 1 < argc) {
            if (parse_double(argv[++i], &config.load_percent, 1.0, 100.0) != 0) {
                fprintf(stderr, "quickbench: invalid --load value\n");
                return EXIT_FAILURE;
            }
        } else {
            fprintf(stderr, "quickbench: unknown or incomplete option '%s'\n", argv[i]);
            return EXIT_FAILURE;
        }
    }

    printf("QuickBench %s\n", QB_VERSION);
    printf("  workload : cpu\n");
    printf("  threads  : %u\n", config.threads);
    printf("  duration : %.3f s\n", config.duration_seconds);
    printf("  warmup   : %.3f s\n", config.warmup_seconds);
    printf("  load     : %.1f%%\n", config.load_percent);
    puts("  running...");

    rc = qb_run(&config, &result);
    if (rc != 0) {
        fprintf(stderr, "quickbench: benchmark failed: %s\n", strerror(rc));
        return EXIT_FAILURE;
    }

    printf("\nResults\n");
    printf("  operations : %llu\n", (unsigned long long)result.operations);
    printf("  elapsed    : %.6f s\n", result.elapsed_seconds);
    printf("  throughput : %.3f GOPS\n",
           result.operations_per_second / 1000000000.0);

    return EXIT_SUCCESS;
}
