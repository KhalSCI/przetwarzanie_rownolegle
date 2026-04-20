# PI4 — prywatny akumulator każdego wątku + jedno `atomic` na końcu

## Cel
Ograniczyć liczbę operacji atomowych z N (PI3: jedna na iterację) do T (jedna na wątek). Przeniesienie kosztu synchronizacji z gorącej pętli poza nią.

## Decyzje projektowe
- **`double sum_local = 0.0`** zadeklarowane wewnątrz bloku `#pragma omp parallel` — automatycznie prywatne. Kompilator z `-O3` trzyma je w rejestrze FPU przez całą pętlę tego wątku.
- **`double x`** zadeklarowane w tym samym bloku — prywatne, nazwa zgodna z oryginałem.
- **`#pragma omp atomic` poza pętlą `omp for`**, ale wciąż w regionie równoległym — każdy wątek dokłada swój wkład dokładnie raz, po zakończeniu swojego zakresu iteracji. Liczba konfliktów ≤ T, niezależna od `num_steps`.

## Analiza zmiennych

| Zmienna | Status | Dostęp |
|---|---|---|
| `i`    | private | rejestr |
| `x`    | **private** (deklarowana w regionie) | rejestr |
| `sum_local` | **private** | rejestr przez całą pętlę |
| `sum`  | shared | atomowy zapis **raz na wątek** |
| `step` | shared | odczyt |

## Faktyczne wyniki pomiarów (num_steps = 10^9)

### T = 20 (procesory logiczne)
```
 3.141592653590 Wartosc liczby PI sekwencyjnie
 3.141592653590 Wartosc liczby PI rownolegle
Czas CPU   sekw : 0.908107 s    |  rown : 2.319185 s
Wall-clock sekw : 0.832444 s    |  rown : 0.127720 s
Przyspieszenie  : 6.518
```

### T = 10 (procesory fizyczne, OMP_PLACES=cores)
```
Wall-clock sekw : 0.821397 s    |  rown : 0.146865 s
Przyspieszenie  : 5.593
```

## Interpretacja wyników

- **Wynik poprawny** do 12 cyfr.
- **Przyspieszenie 6.52× przy 20 wątkach** — znaczący zysk. Daleko od 20× liniowego, ale:
  - i7-12700K ma architekturę **hybrydową**: 8 rdzeni P (Performance, z HT → 16 wątków) + 4 rdzenie E (Efficient, bez HT → 4 wątki). Rdzenie E są wolniejsze → nie skalują się 1:1.
  - Hyperthreading daje zwykle ~1.2–1.3× na rdzeniu P (dwa wątki walczą o FPU), nie 2×.
  - Szacunkowy teoretyczny sufit: `8 × 1.3 + 4 × 0.5 ≈ 12.4×`. Osiągamy ~52% tego sufitu — jest miejsce na poprawę (głównie w synchronizacji na końcu regionu równoległego i dzieleniu pracy).
- **Przyspieszenie 5.59× przy 10 wątkach fizycznych** — **nieoczekiwane: 10 wątków daje mniej niż 20**. Powód: z `OMP_PLACES=cores OMP_PROC_BIND=close` wątki są pinowane po kolei, więc 10 wątków pokrywa 8 × P + 2 × E (~8 szybkich + 2 wolne). Przy 20 wątkach wszystkie rdzenie są w pełni użyte. Dla uczciwego porównania „fizyczne vs logiczne" warto spróbować `OMP_PROC_BIND=spread` i sprawdzić alokację.
- **Czas CPU 2.32 s / wallclock 0.128 s = 18.1** — efektywne wysycenie prawie 18 wątków jednocześnie. Bardzo dobrze — wątki realnie liczą, nie czekają.

## Stopień lokalności dostępu do danych
**Bardzo wysoki.** `sum_local`, `x`, `step`, `i` — wszystko w rejestrach (dzięki `-O3`). Gorąca pętla nie robi dostępów do pamięci ani ruchu koherencji. Stosunek łatwych dostępów do trudnych → bliski 1.

## Kiedy i dlaczego dostępy do pamięci
- W gorącej pętli — praktycznie nigdy. Kompilator trzyma akumulator w rejestrze.
- Na końcu regionu — jedno `atomic` do `sum` na wątek. Cały koszt synchronizacji zamknięty w T operacji.
- Przyczyna końcowego dostępu: scalanie prywatnych wyników w jedną wartość globalną.

## Unieważnianie linii cache
- W gorącej pętli: **brak**.
- Na końcu: T razy (raz na wątek) podczas finalnego scalania — pomijalne w skali całkowitego czasu.

## Czas CPU vs wall-clock (Linux)
- CPU ≈ 2.32 s = suma czasu użycia wszystkich rdzeni — rdzenie **realnie liczą** przez ten czas.
- Wall-clock = 0.128 s = czas ludzki między startem a końcem obliczenia.
- Stosunek CPU/wall ≈ 18 → tyle rdzeni efektywnie pracowało równocześnie. Resztę do 20 pożerają pomniejsze koszty (harmonogram OpenMP, finalny atomic, mniej wydajne rdzenie E).

## Wnioski
Właściwy wzorzec implementacji redukcji ręcznie — działa dobrze. Przyspieszenie 6.5× to realne odzwierciedlenie pracy 20 wątków na CPU hybrydowym. PI5 pokazuje ten sam efekt napisany deklaratywnie przez `reduction`.
