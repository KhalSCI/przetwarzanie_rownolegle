#include <stdio.h>
#include <omp.h>

static void run_case(const char *title, omp_sched_t kind, int chunk, int nthreads, int niter) {
    int i;

    printf("\n=== %s ===\n", title);

    omp_set_num_threads(nthreads);
    omp_set_schedule(kind, chunk);

    #pragma omp parallel default(none) shared(niter)
    {
        #pragma omp single
        {
            printf("threads=%d, niter=%d\n", omp_get_num_threads(), niter);
        }

        /* runtime reads schedule from omp_set_schedule for each test case. */
        #pragma omp for schedule(runtime)
        for (i = 0; i < niter; i++) {
            printf("tid=%d -> iter=%d\n", omp_get_thread_num(), i);
        }
    }
}

int main(void) {
    const int nthreads = 4;
    const int niter = 16;

    run_case("default schedule (implementation-defined)", omp_sched_static, 0, nthreads, niter);
    run_case("static block schedule", omp_sched_static, 0, nthreads, niter);
    run_case("static cyclic schedule, chunk=1", omp_sched_static, 1, nthreads, niter);
    run_case("dynamic schedule (default chunk)", omp_sched_dynamic, 0, nthreads, niter);
    run_case("guided schedule, chunk=2", omp_sched_guided, 2, nthreads, niter);

    return 0;
}
