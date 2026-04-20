# PI3 — `#pragma omp atomic` na zmiennej `sum`

## Cel
Naprawić wyścig z PI2 przez wymuszenie niepodzielnego ciągu read-modify-write na `sum`. Zmienna `x` sprywatyzowana (deklaracja wewnątrz regionu).

## ⚠️ Uwaga o liczbie kroków

`num_steps = 10^7` (100× mniej niż w pozostałych wersjach). Powód: PI3 wykonuje `#pragma omp atomic` w **każdej iteracji**. Dla 10^9 iteracji ta wersja zajęła 40+ sekund — robienie pomiarów dla tylu iteracji nie ma sensu, a wniosek jest ten sam.

**Skutek dla porównań:**
- Przyspieszenie `sewtime / pewtime` w samym PI3 jest liczone wobec **jego własnego** baseline'u sekwencyjnego z 10^7 iteracji — **to jest to, co widać w tabeli wyników**.
- Przy porównaniu z baseline z PI1 (10^9 iteracji) trzeba zeskalować: `T_seq_10^9 / T_par_10^7 ~= 0.83s / (pewtime × 100)`. W praktyce to by dało S << 1 — PI3 jest dramatycznie wolniejsze.

## Decyzje projektowe
- **`double x` zadeklarowane wewnątrz bloku `#pragma omp parallel`** — zmienna lokalna bloku jest automatycznie `private` dla każdego wątku.
- **`#pragma omp atomic`** zamiast `critical` — `atomic` tłumaczy się na jedną instrukcję sprzętową z prefiksem `LOCK` (np. `lock cmpxchg`/`lock xadd`), znacznie tańszą niż mutex `critical`.
- **Brak `reduction`** — celowo, treść zadania wymaga atomowego zapisu do globalnej sumy.

## Analiza zmiennych

| Zmienna | Status | Dostęp |
|---|---|---|
| `i`    | private (omp for) | rejestr |
| `x`    | **private** (deklarowana w regionie) | rejestr |
| `sum`  | shared | atomowy RMW — chroniony |
| `step` | shared | odczyt |

## Konsekwencje `#pragma omp atomic`
- Kompilator generuje **jedną sprzętową instrukcję atomową** z prefiksem `LOCK` na x86.
- Instrukcja `LOCK`-owana:
  - wymusza flush store buffer,
  - serializuje dostęp do linii na szynie koherencji,
  - przed operacją musi unieważnić linię we wszystkich pozostałych rdzeniach,
  - po operacji inny rdzeń musi znów przejąć linię na wyłączność.
- Efektywnie: **sekwencyjny punkt synchronizacji w każdej iteracji**. Wątki są szeregowane na dostępie do `sum`.

## Faktyczne wyniki pomiarów (num_steps = 10^7)

### T = 20 (procesory logiczne)
```
 3.141592653590 Wartosc liczby PI sekwencyjnie
 3.141592653590 Wartosc liczby PI rownolegle
Czas CPU   sekw : 0.009029 s    |  rown : 7.896678 s      (!!)
Wall-clock sekw : 0.008281 s    |  rown : 0.406896 s
Przyspieszenie  : 0.020
```

### T = 10 (procesory fizyczne)
```
Wall-clock sekw : 0.008166 s    |  rown : 0.233677 s
Przyspieszenie  : 0.035
```

## Interpretacja wyników

- **Wynik poprawny** — atomic gwarantuje niepodzielność RMW.
- **Przyspieszenie ≈ 0.02** — PI3 jest **~50× WOLNIEJSZE** od sekwencyjnego (przy T=20). Powód: w każdej iteracji następuje ping-pong linii cache `sum` między 20 rdzeniami. Każda iteracja = 1 instrukcja atomowa + czas na transfer linii (~50–100 ns koherencji).
- **Czas CPU 7.9 s vs wall-clock 0.41 s** → wysycenie 19× na 20 wątkach — rdzenie spędzają większość czasu **czekając w kolejce** na dostęp do linii `sum`. To „zużyty" czas procesorów (busy-wait w sprzętowej kolejce koherencji).
- **10 wątków lepsze niż 20**: mniejsza kontencja → krótszy wall-clock (0.23 s vs 0.41 s). Hyperthreading nie pomaga, gdy ograniczeniem jest szyna koherencyjna.

## Stopień lokalności dostępu do danych
**Bardzo niski** dla `sum` — każda iteracja wymaga sprowadzenia linii i zapisu z unieważnieniem. Stosunek łatwych dostępów do trudnych jest bliski zera.

`x`, `i`, `step` — rejestry (lokalność wysoka), ale to nieistotne przy tak dominującej kontencji.

## Kiedy i dlaczego dostępy do pamięci
- W każdej iteracji — atomowy zapis do `sum` nie może zostać schowany w rejestrze.
- Przyczyna: wymóg atomowości + implicitny flush tej zmiennej w dyrektywie `atomic`.

## Unieważnianie linii cache
**Masywne i dominujące.** Główna przyczyna wysokiego czasu. Linia `sum` „lata" między rdzeniami w **każdej iteracji**.

## Wnioski
Poprawny wynik, katastrofalna wydajność. Atomic na gorącej zmiennej globalnej praktycznie serializuje pętlę i do tego dorzuca koszt koherencji — gorzej niż sekwencyjnie. Żeby uzyskać realne przyspieszenie, trzeba zredukować częstotliwość atomowych operacji: jedna na wątek zamiast jedna na iterację → **PI4**.
