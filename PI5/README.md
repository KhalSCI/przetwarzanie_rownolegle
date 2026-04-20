# PI5 — klauzula `reduction(+:sum)`

## Cel
Wyrazić redukcję idiomatycznie w OpenMP. Kompilator robi automatycznie to, co PI4 ręcznie: tworzy prywatną kopię `sum` w każdym wątku, akumuluje lokalnie, scala na końcu.

## Decyzje projektowe
- **`#pragma omp parallel for private(x) reduction(+:sum)`** — połączona dyrektywa: region + podział pracy + redukcja.
- **`private(x)`** — zmienna pomocnicza, nie współdzielimy. Zachowana nazwa `x` z oryginału.
- **`reduction(+:sum)`** — kompilator:
  1. tworzy prywatną kopię `sum` zainicjalizowaną 0.0,
  2. po zakończeniu iteracji każdy wątek dokłada swoją kopię do wartości globalnej,
  3. scalanie może być drzewiaste (log T) lub liniowe — zależne od implementacji.

## Analiza zmiennych

| Zmienna | Status | Dostęp |
|---|---|---|
| `i`    | private (omp for) | rejestr |
| `x`    | private (klauzula) | rejestr |
| `sum`  | **reduction** — prywatna kopia per wątek | rejestr, scalana na końcu |
| `step` | shared | odczyt |

## Faktyczne wyniki pomiarów (num_steps = 10^9)

### T = 20 (procesory logiczne)
```
 3.141592653590 Wartosc liczby PI sekwencyjnie
 3.141592653590 Wartosc liczby PI rownolegle
Czas CPU   sekw : 0.905626 s    |  rown : 2.225830 s
Wall-clock sekw : 0.830164 s    |  rown : 0.119953 s
Przyspieszenie  : 6.921
```

### T = 10 (procesory fizyczne, OMP_PLACES=cores OMP_PROC_BIND=close)
```
Czas CPU   sekw : 0.908203 s    |  rown : 1.131231 s
Wall-clock sekw : 0.832599 s    |  rown : 0.111464 s
Przyspieszenie  : 7.470
```

## Interpretacja wyników

- **Wynik poprawny** do 12 cyfr.
- **Przyspieszenie 6.92× (T=20) / 7.47× (T=10)** — **najlepsze ze wszystkich wersji PI2–PI6**.
- **Ciekawa obserwacja: 10 wątków > 20 wątków** (7.47× vs 6.92×). To nie błąd — to efekt hybrydowej architektury 12700K:
  - Z 20 wątkami OpenMP rozdziela pracę na 8 rdzeni P × 2 HT + 4 rdzenie E. Pętla jest bardzo homogeniczna, więc **wolniejsze rdzenie E stają się bottleneckiem** — szybkie rdzenie P kończą swoje iteracje i muszą czekać (bariera na końcu regionu) na rdzenie E.
  - Z 10 wątkami z pinowaniem `OMP_PLACES=cores` OpenMP dostaje jeden wątek na rdzeń. Jeśli wszystkie 10 lądują na rdzeniach P (plus 2 na E — ale nie wszystkie zapełnione), praca jest bardziej zrównoważona.
  - Dodatkowo: 10 wątków = mniej kontencji o zasoby współdzielone wewnątrz rdzenia P (FPU, dekoder), każdy wątek dostaje pełny rdzeń.
- **CPU 1.13 s / wall 0.11 s = 10.1×** wysycenia — idealna skala, wszystkie 10 wątków zajęte pracą.

## Stopień lokalności dostępu do danych
**Bardzo wysoki**, identycznie jak PI4. Prywatna kopia `sum` w rejestrze, gorąca pętla bez dostępów do pamięci ani koherencji. Stosunek łatwych dostępów → ≈ 1.

## Kiedy i dlaczego dostępy do pamięci
- W gorącej pętli — nie. Prywatna kopia `sum` żyje w rejestrze.
- Na końcu regionu — każdy wątek zapisuje swoją kopię, potem scalanie (zwykle w wątku master lub drzewiasto).
- Przyczyna: finalne złożenie wyników.

## Unieważnianie linii cache
Minimalne — kilka transakcji na wątek podczas scalania. W gorącej pętli: brak.

## PI4 vs PI5 — porównanie zmierzonych wartości
| | PI4 wall (T=20) | PI5 wall (T=20) | PI4 S | PI5 S |
|---|---|---|---|---|
| | 0.128 s | 0.120 s | 6.52 | **6.92** |

PI5 jest o ~6% szybsze od PI4 (mniej widocznej kontencji na końcowym scalaniu — kompilator może zastosować redukcję drzewiastą w `reduction`, a ręczny atomic w PI4 jest liniowy).

Dla 10 wątków fizycznych: **PI5 7.47× vs PI4 5.59×** — różnica większa, pewnie wynik wpływu pinowania na konkretnie tej maszynie (powtórzyć kilka razy, żeby potwierdzić).

## Czas CPU vs wall-clock (Linux)
T=20: CPU 2.23 s, wall 0.12 s → ~19× wysycenia. T=10: 10.1× wysycenia. Wątki realnie liczą, nie czekają.

## Wnioski
`reduction` to właściwy sposób pisania tego typu kodu. Najczystsze, najkrótsze i najszybsze. **Najlepsze przyspieszenie** z całego zestawu PI1–PI6. Używać ilekroć operacja jest łączna i przemienna (`+`, `*`, `max`, `min` itd.).
