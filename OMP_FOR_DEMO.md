# OpenMP `for` - krotkie wyjasnienie

Plik [omp_for_demo.c](omp_for_demo.c) pokazuje, jak `#pragma omp for` dzieli iteracje petli miedzy watki.

## Co robi `#pragma omp for`

- Bez `omp for`: kazdy watek wykona cala petle.
- Z `omp for`: iteracje sa rozdzielone miedzy watki, wiec kazda iteracja wykona sie raz.

W programie wypisywany jest identyfikator watku (`tid`) i numer iteracji (`iter`), dzieki temu widzisz podzial pracy.

## Warianty harmonogramu (`schedule`)

- `default`: domyslny wybor runtime/kompilatora (moze zalezec od implementacji).
- `static` (blokowy): kazdy watek dostaje ciagly blok iteracji.
- `static,1` (cykliczny): iteracje sa rozdawane naprzemiennie miedzy watki.
- `dynamic`: watki pobieraja kolejne paczki iteracji w trakcie pracy.
- `guided`: na poczatku duze paczki, potem coraz mniejsze.

## Kompilacja i uruchomienie

```bash
gcc -fopenmp omp_for_demo.c -o omp_for_demo -O2
./omp_for_demo
```

## Jak czytac wynik

- `tid=2 -> iter=9` oznacza: iteracja 9 zostala wykonana przez watek 2.
- Kolejnosc linii bywa rozna miedzy uruchomieniami (to normalne w programie rownoleglym).
- Najwazniejsze: kazdy numer iteracji powinien pojawic sie tylko raz w ramach jednego testu.
