/*
 * PI6 - tablica sum czesciowych w pamieci wspoldzielonej (FALSE SHARING)
 *
 * Sasiednie elementy tab[] modyfikowane przez rozne watki leza w tej samej
 * linii cache -> unieważnianie linii w L1d innych rdzeni w kazdej iteracji.
 *
 * volatile + #pragma omp flush(tab) wymuszaja, zeby zapisy realnie szly do
 * pamieci (bez tego kompilator trzymalby tab[id] w rejestrze i false sharing
 * nie wystapilby).
 *
 * Plik liczy PI w JEDNYM uruchomieniu: najpierw sekwencyjnie, potem rownolegle.
 *
 * Kompilacja: gcc -O3 -fopenmp pi.c -o pi6
 * Zalecane:   OMP_PLACES=cores OMP_PROC_BIND=spread  (rozne rdzenie fizyczne)
 */
#include <stdio.h>
#include <time.h>
#include <omp.h>

#define MAX_THREADS 64

long long num_steps = 1000000000;
double step;

int main(int argc, char* argv[])
{
    clock_t spstart, spstop, ppstart, ppstop;
    double sswtime, sewtime, pswtime, pewtime;
    double x, pi, sum = 0.0;
    long long i;
    int t, nthreads = 0;

    volatile double tab[MAX_THREADS];
    for (t = 0; t < MAX_THREADS; t++) tab[t] = 0.0;

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

    /* ======================= ROWNOLEGLE (PI6) =================== */
    sum = 0.0;
    pswtime = omp_get_wtime();
    ppstart = clock();

    #pragma omp parallel
    {
        int id = omp_get_thread_num();
        double x;                       /* prywatne dla watku */

        #pragma omp single
        nthreads = omp_get_num_threads();

        #pragma omp for
        for (i = 0; i < num_steps; i++) {
            x = (i + 0.5) * step;
            tab[id] = tab[id] + 4.0 / (1.0 + x * x);
            #pragma omp flush(tab)      /* wzmocnienie false sharing */
        }
    }

    for (t = 0; t < nthreads; t++) sum += tab[t];
    pi = sum * step;

    ppstop  = clock();
    pewtime = omp_get_wtime();
    printf("%15.12f Wartosc liczby PI rownolegle  <-- PI6: tablica sum, false sharing\n", pi);

    /* ========================= WYNIKI =========================== */
    printf("Liczba watkow                          : %d\n",  nthreads);
    printf("Czas procesorow sekwencyjnie           : %f s\n", (double)(spstop  - spstart) / CLOCKS_PER_SEC);
    printf("Czas procesorow rownolegle             : %f s\n", (double)(ppstop  - ppstart) / CLOCKS_PER_SEC);
    printf("Czas trwania obliczen sekw - wallclock : %f s\n", sewtime - sswtime);
    printf("Czas trwania obliczen rown - wallclock : %f s\n", pewtime - pswtime);
    printf("Przyspieszenie                         : %5.3f\n", (sewtime - sswtime) / (pewtime - pswtime));
    return 0;
}
