/*
 * PI2 - region rownolegly + podzial petli, BEZ ochrony wspoldzielonej sumy
 * (celowo bledna wersja - pokazuje race condition)
 *
 * Plik liczy PI w JEDNYM uruchomieniu: najpierw sekwencyjnie, potem rownolegle,
 * drukuje czasy i przyspieszenie.
 *
 * Kompilacja: gcc -O3 -fopenmp pi.c -o pi2
 * Uruchomienie:
 *   OMP_NUM_THREADS=20 ./pi2   # liczba procesorow logicznych
 *   OMP_NUM_THREADS=10 ./pi2   # liczba procesorow fizycznych
 */
#include <stdio.h>
#include <time.h>
#include <omp.h>

long long num_steps = 1000000000;
double step;

int main(int argc, char* argv[])
{
    clock_t spstart, spstop, ppstart, ppstop;
    double sswtime, sewtime, pswtime, pewtime;
    double x, pi, sum = 0.0;
    long long i;

    step = 1.0 / (double)num_steps;

    /* ======================= SEKWENCYJNIE ======================= */
    sswtime = omp_get_wtime();
    spstart = clock();

    for (i = 0; i < num_steps; i++) {
        x = (i + 0.5) * step;
        sum = sum + 4.0 / (1.0 + x * x);
    }
    pi = sum * step;

    spstop  = clock();
    sewtime = omp_get_wtime();
    printf("%15.12f Wartosc liczby PI sekwencyjnie\n", pi);

    /* ======================= ROWNOLEGLE (PI2) =================== */
    sum = 0.0;
    pswtime = omp_get_wtime();
    ppstart = clock();

    #pragma omp parallel
    {
        #pragma omp for
        for (i = 0; i < num_steps; i++) {
            x = (i + 0.5) * step;           /* x  SHARED => race */
            sum = sum + 4.0 / (1.0 + x * x); /* sum SHARED => race */
        }
    }
    pi = sum * step;

    ppstop  = clock();
    pewtime = omp_get_wtime();
    printf("%15.12f Wartosc liczby PI rownolegle  <-- PI2 bez ochrony, zwykle BLEDNY\n", pi);

    /* ========================= WYNIKI =========================== */
    printf("Liczba watkow                          : %d\n",  omp_get_max_threads());
    printf("Czas procesorow sekwencyjnie           : %f s\n", (double)(spstop  - spstart) / CLOCKS_PER_SEC);
    printf("Czas procesorow rownolegle             : %f s\n", (double)(ppstop  - ppstart) / CLOCKS_PER_SEC);
    printf("Czas trwania obliczen sekw - wallclock : %f s\n", sewtime - sswtime);
    printf("Czas trwania obliczen rown - wallclock : %f s\n", pewtime - pswtime);
    printf("Przyspieszenie                         : %5.3f\n", (sewtime - sswtime) / (pewtime - pswtime));
    return 0;
}
