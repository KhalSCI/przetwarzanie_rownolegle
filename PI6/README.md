# PI6 — tablica sum częściowych we wspólnej pamięci (false sharing)

## Cel
Zademonstrować zjawisko **false sharing**: każdy wątek ma „swoją" komórkę w tablicy, ale sąsiednie komórki mieszczą się w tej samej linii cache — zapisy jednego wątku unieważniają linię u innych, mimo że logicznie wątki nie kolidują.

## Decyzje projektowe
- **`volatile double tab[MAX_THREADS]`** — `volatile` zabrania kompilatorowi trzymać wartości w rejestrze. Bez tego `-O3` przeniósłby `tab[id]` do rejestru i false sharing **by nie wystąpił**.
- **`#pragma omp flush(tab)` w każdej iteracji** — dodatkowe wzmocnienie: jawny rozkaz synchronizacji obrazu prywatnego z pamięcią.
- **Zmienna `x`** zadeklarowana wewnątrz regionu — prywatna, nazwa zgodna z oryginałem.
- **`MAX_THREADS = 64`** — górny limit; CPU ma 20 wątków.

## Analiza zmiennych

| Zmienna | Status | Dostęp |
|---|---|---|
| `i`    | private | rejestr |
| `x`    | **private** | rejestr |
| `id`   | private | rejestr |
| `tab[id]` | **shared array**, `volatile` | zapis w pamięci w każdej iteracji |
| `step` | shared | odczyt |

## False sharing — mechanizm

1. Linia L1d w i7-12700K = **64 B** = 8 × `double`.
2. `tab[0..7]` leży w jednej linii, `tab[8..15]` w następnej itd.
3. Wątek 0 pisze do `tab[0]`. Linia w stanie **Modified** u rdzenia 0.
4. Wątek 1 pisze do `tab[1]` (ta sama linia!). Linia transfer do rdzenia 1, unieważnienie u rdzenia 0.
5. Wątek 0 w następnej iteracji znów pisze do `tab[0]`. Transfer z powrotem, unieważnienie u rdzenia 1.
6. **Linia „lata" między rdzeniami w każdej iteracji.**

Z punktu widzenia logiki nic złego się nie dzieje; sprzęt widzi jednak konflikt o linię.

## Faktyczne wyniki pomiarów (num_steps = 10^9)

### T = 20 (procesory logiczne)
```
 3.141592653590 Wartosc liczby PI sekwencyjnie
 3.141592653590 Wartosc liczby PI rownolegle
Czas CPU   sekw : 0.889787 s    |  rown : 259.150827 s   (!!)
Wall-clock sekw : 0.815650 s    |  rown : 13.126064 s
Przyspieszenie  : 0.062
```

### T = 10 (procesory fizyczne, OMP_PLACES=cores OMP_PROC_BIND=spread)
```
Czas CPU   sekw : 0.871730 s    |  rown : 169.024753 s
Wall-clock sekw : 0.799089 s    |  rown : 16.194528 s
Przyspieszenie  : 0.049
```

## Interpretacja wyników

- **Wynik poprawny** — każdy wątek pisze tylko do swojej komórki, logicznie nie ma race.
- **Przyspieszenie ≈ 0.05** — PI6 jest **~20× WOLNIEJSZE** od sekwencyjnego. Dramatyczne spowolnienie, **pomimo poprawnego kodu**.
- **Czas CPU 259 s vs wall-clock 13 s przy 20 wątkach** → wysycenie 19.7×. Wszystkie rdzenie „pracują", ale większość czasu to oczekiwanie na aktualne linie cache — ping-pong w protokole koherencji.
- **Porównanie z PI4 (identyczna logika, ale z `sum_local` w rejestrze zamiast `tab[id]` w pamięci)**:
  - PI4 wall-clock = 0.128 s
  - PI6 wall-clock = 13.126 s
  - **PI6 jest 102× wolniejsze od PI4!**
  - Jedyna różnica: `volatile` i zapisy do sąsiednich słów. Cały narzut to false sharing.
- **T=10 spread wolniejsze niż T=20**: przy `OMP_PROC_BIND=spread` wątki są rozprzestrzenione na różne rdzenie fizyczne, więc każdy zapis gwarantowanie wywołuje transfer linii między L1d różnych rdzeni. Przy T=20 i `close` część wątków HT dzieli L1d — tam false sharing się *nie materializuje* (wspólny cache), częściowo łagodzi problem.

## Dlaczego false sharing może NIE wystąpić

1. **Optymalizacja kompilatora** — bez `volatile` `-O3` trzyma `tab[id]` w rejestrze. Dzięki `volatile` + `flush` kod faktycznie pisze do pamięci.
2. **Dwa wątki na jednym rdzeniu HT** — hyperthreading współdzieli L1d, więc zapisy dwóch wątków HT do tej samej linii NIE wywołują ruchu koherencyjnego. Widać to w pomiarze T=20/close — czas jest niższy niż T=10/spread, bo część par wątków dzieli L1.

## Stopień lokalności dostępu do danych
**Katastrofalnie niski** dla `tab[id]` — każdy zapis generuje ruch koherencyjny. Rejestr/cache hit tylko dla `x`, `i`, `id`, `step`. Stosunek łatwych/trudnych dostępów znacznie gorszy niż PI4/PI5.

## Kiedy i dlaczego dostępy do pamięci
- W każdej iteracji — `volatile` + `flush(tab)` wymuszają zapis i odczyt z pamięci.
- Przyczyna: semantyka `volatile` + jawny flush.

## Unieważnianie linii cache
**Występuje w każdej iteracji**, na każdej parze (rdzeń_A, rdzeń_B) pracującej na tej samej linii. Dominująca przyczyna wysokiego czasu.

## Czas CPU vs wall-clock (Linux)
- T=20: CPU 259 s, wall 13 s → stosunek ~19.7 — wszystkie rdzenie są zajęte, ale zajmuje je transfer linii.
- T=10: CPU 169 s, wall 16 s → stosunek ~10.4 — mniej równoległości, więcej faktycznej pracy na wątek (mniejsza wielkość tablicy do ping-pongu).

## Jak to naprawić (informacyjnie — niewymagane)
Wyrównać komórki do granicy linii (`alignas(64)` lub padding 8 `double` między komórkami). Każda komórka w swojej linii → false sharing znika → wydajność wraca do PI4/PI5.

## Wnioski
Poprawne obliczenia, dramatycznie zła wydajność. Klasyczna pułapka pamięci współdzielonej — **pozornie prywatne dane dzielą linie cache i zabierają całe przyspieszenie**. PI7 wykorzystuje to zjawisko jako narzędzie pomiaru długości linii cache.
