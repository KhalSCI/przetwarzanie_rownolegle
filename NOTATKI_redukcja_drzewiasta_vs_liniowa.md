# Notatka — redukcja liniowa (PI4) vs drzewiasta (PI5)

## Co się scala

Na końcu regionu każdy z T wątków ma prywatny `sum_local`. Trzeba zsumować T liczb do jednej globalnej `sum`.

## Liniowa (PI4) — kolejka na `#pragma omp atomic`

Każdy wątek robi `sum += sum_local` pod `atomic`. 20 wątków walczy o linię cache z `sum` → ustawiają się w kolejkę, jeden po drugim.

```
Czas →

wątek 0:  [atomic: sum += s0]
wątek 1:                      [wait][atomic: sum += s1]
wątek 2:                                                [wait][atomic: sum += s2]
...
wątek 19:                                                                         ...[atomic: sum += s19]

Łączny czas scalania:  T × t_atomic
```

**Złożoność: O(T)**

## Drzewiasta (PI5, potencjalnie)

Runtime OpenMP może scalać parami, pary par, itd.:

```
  s0 s1  s2 s3  s4 s5  s6 s7  s8 s9  ... s18 s19
   \_/    \_/    \_/    \_/    \_/         \_/
    +      +      +      +      +           +        ← krok 1 (10 par RÓWNOLEGLE)
    |______|      |______|      |___________|
       +             +                +                ← krok 2 (5 równolegle)
       |_____________|                |
              +                       |                ← krok 3
              |_______________________|
                        +                              ← krok 4
                        ...
                       sum                             ← krok 5
```

**Złożoność: O(log T)** — dla T=20 → ~5 kroków zamiast 20.

## Porównanie złożoności scalania

| T | Liniowa | Drzewiasta |
|---|---|---|
| 4  | 4 kroków | 2 kroków |
| 20 | 20 | 5 |
| 64 | 64 | 6 |
| 256 | 256 | 8 |

Im więcej wątków, tym większa przewaga drzewa.

## ALE — to NIE tłumaczy 6% różnicy między PI4 a PI5 w naszych pomiarach

Szczera kalkulacja dla T=20:
- `t_atomic` ≈ 50–100 ns
- Liniowa: 20 × 100 ns = **2 μs**
- Drzewiasta: 5 × 100 ns = **500 ns**
- Różnica bezwzględna: **~1.5 μs**
- Zmierzony czas pętli: **120 ms**
- Udział scalania: **~0.001%** — **niewidoczne w pomiarze**

## Skąd więc ~6% różnicy PI5 vs PI4

1. **Inny kod kompilatora** — `reduction` może być inline'owana bez wywołania funkcji runtime, `atomic` idzie przez libgomp.
2. **Bariery i synchronizacja** — PI4 ma domyślną barierę końca `omp for` + kolejkowanie na `atomic`; w PI5 to jedna zunifikowana operacja.
3. **Szum pomiarowy** — 6% przy 120 ms = ~7 ms, w zasięgu jitteru systemu. Żeby potwierdzić, trzeba kilkanaście powtórzeń i mediana.

## Kiedy tree vs linear REALNIE się liczy

- **Dużo wątków** (64+, Threadripper, serwery) — log T << T
- **Krótka pętla** — scalanie przestaje być pomijalne
- **Region otwierany wielokrotnie** — koszt scalania się kumuluje

Dla 10⁹ iteracji na 20-rdzeniowym 12700K ta różnica po prostu **nie ma tej skali, żeby była widoczna**.

## TL;DR

```
Kolejka (PI4):      s0 → s1 → s2 → ... → s19       (T kroków sekwencyjnie)
Drzewo (PI5):       parami → parami → ...           (log T kroków, na każdym RÓWNOLEGLE)
```

- **Teoretycznie:** drzewo szybsze o czynnik T / log T.
- **Praktycznie dla naszych pomiarów:** różnica 6% nie pochodzi z redukcji — pochodzi z innych źródeł (kod, bariery, szum).
- **Drzewo zaczyna się opłacać** przy wielu wątkach i krótkiej pracy — nie tu.
