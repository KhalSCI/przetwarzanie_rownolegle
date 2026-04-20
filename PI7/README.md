# PI7 — eksperymentalne wyznaczenie długości linii pamięci podręcznej

## Cel
Wykorzystać zjawisko **false sharing** jako narzędzie pomiarowe. Dwa wątki pracują na sąsiednich słowach jednej tablicy. Mierzymy czas obliczeń dla par `(tab[k], tab[k+1])`, `k = 0..N-2`. Gdy oba słowa leżą w jednej linii cache — długi czas (unieważnianie). Gdy w różnych liniach — krótki czas. Odległość między kolejnymi „krótkimi" pomiarami = długość linii.

## Teoria

### Linia pamięci podręcznej
Cache line — najmniejsza jednostka transferu między RAM i L1/L2/L3 oraz jednostka śledzenia koherencji. Na x86 (w tym 12th Gen) **linia = 64 bajty = 8 słów `double`**.

### False sharing
Logicznie niezależne zmienne różnych wątków leżą w tej samej linii cache. Zapis jednego wątku unieważnia linię u pozostałych — każdy kolejny dostęp wymaga cache-to-cache transferu. Sprzęt nie widzi logicznej niezależności, widzi tylko linię.

## Decyzje projektowe
- **`volatile double tab[N_SLOTS]`** — warunek konieczny. Bez tego `-O3` trzymałby `tab[idx]` w rejestrze → false sharingu by nie było.
- **`#pragma omp flush(tab)` w każdej iteracji** — druga warstwa ochrony.
- **`__attribute__((aligned(64)))`** — wyrównuje początek tablicy do granicy linii. `tab[0]` jest początkiem linii, `tab[8]` początkiem następnej itd. Pomiar wypisuje `&tab[0] % 64` — sprawdzamy, czy wyrównanie wyszło.
- **`N_SLOTS = 50`** — ok. 400 B, ~6 linii po 64 B. Wystarczy na kilka cykli.
- **`omp_set_num_threads(2)`** — dokładnie 2 wątki. Tylko dwa wątki są potrzebne (i możliwe) do tego eksperymentu.
- **Pinowanie `OMP_PLACES=cores OMP_PROC_BIND=spread`** — wymagane. Oba wątki na różnych rdzeniach fizycznych (różnych L1d), inaczej eksperyment zawodzi.
- **Podział pracy ręczny (`j_start`, `j_end`)** zamiast `#pragma omp for` — każdy wątek aktualizuje tylko swoje słowo, niezależnie od harmonogramu. Prosto i deterministycznie.

## Faktyczne wyniki pomiarów

```
Adres tab[0]  = 0x61e0e8b15080  (mod 64 = 0)    ← wyrównanie OK
Liczba watkow = 2  |  PI_STEPS_PER_RUN = 50000000

| iter k | tab[k], tab[k+1] | czas [s] |
|      0 |   tab[ 0],tab[ 1] |   0.6825 |   <- długi (jedna linia)
|      1 |   tab[ 1],tab[ 2] |   0.7571 |   <- długi
|      2 |   tab[ 2],tab[ 3] |   0.7393 |
|      3 |   tab[ 3],tab[ 4] |   0.7548 |
|      4 |   tab[ 4],tab[ 5] |   0.7656 |
|      5 |   tab[ 5],tab[ 6] |   0.7605 |
|      6 |   tab[ 6],tab[ 7] |   0.7452 |
|      7 |   tab[ 7],tab[ 8] |   0.1234 |   ← KROTKI — granica linii (L0|L1)
|      8 |   tab[ 8],tab[ 9] |   0.6327 |
|      9 |   tab[ 9],tab[10] |   0.6152 |
|     10 |   tab[10],tab[11] |   0.6514 |
|     11 |   tab[11],tab[12] |   0.6365 |
|     12 |   tab[12],tab[13] |   0.6325 |
|     13 |   tab[13],tab[14] |   0.6403 |
|     14 |   tab[14],tab[15] |   0.6268 |
|     15 |   tab[15],tab[16] |   0.1220 |   ← KROTKI — granica (L1|L2)
|     16 |   tab[16],tab[17] |   0.6299 |
...
|     23 |   tab[23],tab[24] |   0.1228 |   ← KROTKI — granica (L2|L3)
|     24 |   tab[24],tab[25] |   0.6283 |
...
|     31 |   tab[31],tab[32] |   0.1229 |   ← KROTKI — granica (L3|L4)
|     32 |   tab[32],tab[33] |   0.6033 |
...
|     39 |   tab[39],tab[40] |   0.1275 |   ← KROTKI — granica (L4|L5)
|     40 |   tab[40],tab[41] |   0.6001 |
...
|     47 |   tab[47],tab[48] |   0.1324 |   ← KROTKI — granica (L5|L6)
|     48 |   tab[48],tab[49] |   0.5700 |
```

## Wynik eksperymentu

**Outliery (czas krótki) pojawiają się przy `k = 7, 15, 23, 31, 39, 47`.**

Odstępy: 8, 8, 8, 8, 8 — **dokładnie co 8 iteracji**.

### Wzór

```
długość linii [B] = (liczba iteracji długich + liczba iteracji krótkich w cyklu) × sizeof(double)
                  = (7 + 1) × 8 B
                  = 8 × 8 B
                  = 64 B
```

Albo równoważnie:
```
długość linii = odstęp_między_outlierami × sizeof(double) = 8 × 8 B = 64 B
```

**Wynik: linia pamięci podręcznej = 64 B** ✓ (zgodne z oczekiwaniem dla x86).

## Analiza czasów
- Czas „długi" (false sharing): ~0.60–0.77 s
- Czas „krótki" (różne linie): ~0.12–0.13 s
- **Stosunek: ~5–6×** — każdy krok w trybie false sharing jest ~5–6× wolniejszy niż bez niego. To doskonale ilustruje koszt pingu-ponga linii cache między rdzeniami.

Zauważalna tendencja: czas „długi" delikatnie maleje w miarę zwiększania `k` (0.76 → 0.60). Prawdopodobnie efekt rozgrzewania systemu / boost CPU w trakcie uruchomienia. Nie wpływa na interpretację (granice są jednoznaczne).

## Pierwszy outlier przy k=7, nie k=0
Dlaczego `k=7` a nie `k=0`? `tab[0]` i `tab[1]` leżą w **tej samej** linii (obie < 64 B od początku). Dopiero `tab[7]` i `tab[8]` są rozdzielone granicą linii (bajty 56–63 vs 64–71).

Pierwsza para w **różnych** liniach to właśnie `(tab[7], tab[8])` — stąd pierwszy outlier przy k=7. To potwierdza, że `tab[0]` jest dokładnie na początku linii (`aligned(64)` zadziałał, co widać również w wyświetlonym `mod 64 = 0`).

## Warunki konieczne udanego eksperymentu — wszystkie spełnione

| Warunek | Status |
|---|---|
| Jedna ciągła tablica przez wszystkie iteracje | ✓ |
| Dokładnie 2 wątki | ✓ (`omp_set_num_threads(2)`) |
| Dwa różne rdzenie fizyczne | ✓ (`OMP_PLACES=cores OMP_PROC_BIND=spread`) |
| Zapisy realnie lądujące w pamięci | ✓ (`volatile` + `flush`) |
| Wyrównanie do granicy linii | ✓ (`aligned(64)`, potwierdzone `mod 64 = 0`) |

## Wnioski
Eksperyment zadziałał zgodnie z przewidywaniem teoretycznym. Znalezione **6 kolejnych granic** linii cache (outlieri przy k=7, 15, 23, 31, 39, 47), odstęp dokładnie 8 słów `double` za każdym razem.

**Długość linii pamięci podręcznej procesora Intel Core i7-12700K (L1d, pomiar z false sharing) = 64 bajty.**
