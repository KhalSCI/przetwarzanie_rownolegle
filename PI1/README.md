# PI1 — wersja sekwencyjna (baseline)

## Cel
Punkt odniesienia dla wszystkich kolejnych wersji równoległych. Metoda całkowania numerycznego ¼ koła o promieniu 1 (obszar pi/4). Pi wyznaczamy jako sumę `4/(1+x²)*step` dla `x = (i+0.5)*step`, `i = 0..num_steps-1`.

## Decyzje projektowe

- **`num_steps = 10^9`** — tak dobrana liczba kroków, żeby czas obliczeń był liczony w pojedynczych sekundach (zgodnie z treścią zadania). Ta sama liczba używana we wszystkich wersjach (poza PI3, patrz README PI3).
- **`-O3`** — zgodnie z treścią zadania maksymalizacja prędkości i minimalizacja liczby dostępów do pamięci. Kompilator trzyma `sum`, `x` w rejestrach, `step` również.
- **`clock()`** — mierzy czas CPU (sumę wszystkich użytych rdzeni). W wersji sekwencyjnej `clock() ≈ wall-clock`.
- **`omp_get_wtime()`** — mierzy czas rzeczywisty (wall-clock).
- **Nazwy zmiennych** zgodne z oryginalnym `pi.c`: `spstart/spstop` (clock), `sswtime/sewtime` (wall time), `x`, `pi`, `sum`, `step`, `i`.

## Faktyczne wyniki pomiarów (Intel i7-12700K, Linux, gcc -O3)

```
PI1 (sekwencyjnie) = 3.141592653589971
Czas CPU      : 0.905441 s
Wall-clock    : 0.829995 s
```

- Wynik poprawny do ~12 cyfr.
- Czas CPU (0.905 s) ≈ wall-clock (0.830 s) — różnica pochodzi z tego, że `clock()` zlicza także drobny overhead/jitter OS; `omp_get_wtime()` mierzy rzeczywisty upływ czasu z monotonicznego zegara. W pojedynczym wątku oba powinny być zbliżone.

**Ten wall-clock 0.830 s jest mianownikiem w obliczeniu przyspieszenia dla wersji PI2–PI6** (gdy używają tego samego `num_steps = 10^9`).

## Stopień lokalności dostępu do danych
Bardzo wysoki (praktycznie 1). Dzięki `-O3`:
- `sum` — trzymana w rejestrze FPU przez cały czas pętli,
- `x` — rejestr,
- `step` — rejestr,
- `i` — rejestr licznikowy.

Gorąca pętla nie robi dostępów do pamięci. Dostęp do pamięci następuje tylko przed pętlą (załadowanie `step`) i po pętli (zapis `pi`).

## Unieważnianie linii cache
Nie występuje — pojedynczy wątek, brak współdzielenia. Jedna kopia danych w L1d rdzenia wykonującego program.

## Czas CPU vs wall-clock
W wersji sekwencyjnej praktycznie równe (z dokładnością do przerwań OS). Program jest jednowątkowy, więc jeden rdzeń pracuje przez cały czas trwania obliczeń. Zgodne z pomiarem: **CPU 0.905 s, wall-clock 0.830 s**.

## Wnioski
PI1 osiąga maksymalną wydajność jednowątkową (~1.2 mld iteracji/s w tej pętli). Wersje PI2–PI6 porównujemy z tym wynikiem — przyspieszenie T_seq / T_par mówi, ile razy skróciliśmy czas dzięki równoległości (lub o ile razy pogorszyliśmy z powodu kosztów synchronizacji/koherencji).
