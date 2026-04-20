/*
 * PI1 - wersja sekwencyjna (baseline)
 * Kompilacja: gcc -O3 -fopenmp pi.c -o pi1
 * Uruchomienie: ./pi1
 */
#include <stdio.h>
#include <time.h>
#include <omp.h>

long long num_steps = 1000000000;
double step;

int main(int argc, char* argv[])
{
    clock_t spstart, spstop;
    double sswtime, sewtime;
    double x, pi, sum = 0.0;
    long long i;

    sswtime = omp_get_wtime();
    spstart = clock();

    step = 1.0 / (double)num_steps;

    for (i = 0; i < num_steps; i++) {
        x = (i + 0.5) * step;
        sum = sum + 4.0 / (1.0 + x * x);
    }

    pi = sum * step;

    spstop  = clock();
    sewtime = omp_get_wtime();

    printf("PI1 (sekwencyjnie) = %.15f\n", pi);
    printf("Czas CPU      : %f s\n", (double)(spstop - spstart) / CLOCKS_PER_SEC);
    printf("Wall-clock    : %f s\n", sewtime - sswtime);
    return 0;
}
