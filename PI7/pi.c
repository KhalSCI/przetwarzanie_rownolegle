/*
 * PI7 - eksperyment wyznaczenia dlugosci linii pamieci podrecznej procesora
 *
 * Dwa watki pracuja rownolegle na sasiednich slowach tej samej tablicy tab[].
 * Dla kazdej pary (tab[k], tab[k+1]) wielokrotnie obliczaja pi aktualizujac
 * swoje slowa. Gdy oba slowa leza w tej samej linii pp -> false sharing -> duzo
 * unieważniania -> DLUGI czas. Gdy slowa leza w dwoch liniach -> brak false
 * sharingu -> KROTKI czas.
 *
 * Odleglosc miedzy kolejnymi KROTKIMI czasami = dlugosc linii pp.
 *
 * Wymagania eksperymentu:
 *  - dwa watki,
 *  - dwa rozne procesory fizyczne (wymagaja roznych L1d):
 *      OMP_NUM_THREADS=2 OMP_PLACES=cores OMP_PROC_BIND=spread ./pi7
 *  - volatile + flush zeby zapisy realnie sie materializowaly w pamieci,
 *  - zalozenie: tab[0] wyrownane do poczatku linii (sprawdzamy printem).
 *
 * Kompilacja: gcc -O3 -fopenmp pi.c -o pi7
 */
#include <stdio.h>
#include <stdint.h>
#include <omp.h>

#define N_SLOTS          50                /* ok. 50 slow * 8B = 400 B >= 6 linii 64B */
#define PI_STEPS_PER_RUN 50000000          /* krotsza petla - kazda iteracja eksperymentu
                                              liczy pi osobno; razem ok. 50 * T_par */

static volatile double tab[N_SLOTS] __attribute__((aligned(64)));

int main(int argc, char* argv[])
{
    long long num_steps = PI_STEPS_PER_RUN;
    double step = 1.0 / (double)num_steps;
    int k;

    /* wymuszamy dokladnie 2 watki */
    omp_set_num_threads(2);

    printf("PI7: eksperyment wyznaczenia dlugosci linii pamieci podrecznej\n");
    printf("Adres tab[0]  = %p  (mod 64 = %lu)\n",
           (void*)&tab[0], (unsigned long)((uintptr_t)&tab[0] % 64));
    printf("Liczba watkow = 2  |  PI_STEPS_PER_RUN = %lld\n", num_steps);
    printf("Zalecenie: OMP_PLACES=cores OMP_PROC_BIND=spread\n\n");
    printf("| iter k | tab[k], tab[k+1] | czas [s] |\n");
    printf("|--------|------------------|----------|\n");

    for (k = 0; k < N_SLOTS - 1; k++) {
        /* reset slow uzywanych w tej iteracji */
        tab[k]     = 0.0;
        tab[k + 1] = 0.0;

        double t0 = omp_get_wtime();

        #pragma omp parallel
        {
            int id = omp_get_thread_num();   /* 0 lub 1 */
            int idx = k + id;                /* watek 0 -> tab[k], watek 1 -> tab[k+1] */
            double x;
            long long j;

            /* kazdy watek liczy pol przedzialu; obaj aktualizuja SWOJE slowo w tablicy */
            long long j_start = (id == 0) ? 0 : num_steps / 2;
            long long j_end   = (id == 0) ? num_steps / 2 : num_steps;

            for (j = j_start; j < j_end; j++) {
                x = (j + 0.5) * step;
                tab[idx] = tab[idx] + 4.0 / (1.0 + x * x);
                #pragma omp flush(tab)       /* wymuszenie zapisu do pamieci -> false sharing widoczny */
            }
        }

        double t1 = omp_get_wtime();
        double pi = (tab[k] + tab[k + 1]) * step;

        printf("| %6d |   tab[%2d],tab[%2d] | %8.4f |   pi=%.6f\n",
               k, k, k + 1, t1 - t0, pi);
    }

    printf("\nInterpretacja:\n");
    printf("  - KROTKI czas (outlier w dol) dla iteracji k oznacza, ze tab[k] i tab[k+1]\n");
    printf("    leza w DWOCH roznych liniach cache -> brak false sharingu.\n");
    printf("  - DLUGI czas = oba slowa w jednej linii -> false sharing.\n");
    printf("  - Odleglosc miedzy kolejnymi k o krotkim czasie = liczba slow w linii.\n");
    printf("  - Dlugosc linii [B] = (liczba slow miedzy outlierami) * sizeof(double)\n");
    printf("    Przy 8B slowie i linii 64B oczekujemy outliera co 8 iteracji.\n");
    return 0;
}
