# PI2 — `#pragma omp parallel` + `#pragma omp for` (naiwna równoległość)

## Cel
Pokazać, co się dzieje, kiedy dzielimy pracę między wątki BEZ ochrony zmiennych współdzielonych. Wersja celowo błędna — obserwacja wyścigów i ich skutków.

## Decyzje projektowe
- Tylko `#pragma omp parallel` + `#pragma omp for` — zgodnie z treścią zadania, bez `private`, `reduction`, `atomic`, `critical`.
- `x` i `sum` pozostają domyślnie `shared` — bo są zadeklarowane poza regionem równoległym. To źródło problemu.
- `i` automatycznie prywatne (zmienna pętli `omp for`).
- Nazwy zmiennych zgodne z oryginalnym `pi.c`.

## Analiza zmiennych

| Zmienna | Domyślny status | Dostęp |
|---|---|---|
| `i`    | private (pętla `omp for`) | zapis (licznik) |
| `x`    | **shared** | zapis i odczyt — **race** |
| `sum`  | **shared** | read-modify-write — **race** |
| `step` | shared | tylko odczyt (bezpieczne) |

## Faktyczne wyniki pomiarów

### T = 20 (procesory logiczne)
```
 3.141592653590 Wartosc liczby PI sekwencyjnie
 0.199833582888 Wartosc liczby PI rownolegle  <-- BLEDNY
Czas CPU   sekw : 0.882616 s    |  rown : 2.242800 s
Wall-clock sekw : 0.809078 s    |  rown : 0.126660 s
Przyspieszenie  : 6.388
```

### T = 10 (procesory fizyczne, OMP_PLACES=cores OMP_PROC_BIND=close)
```
 0.356198330538 Wartosc liczby PI rownolegle  <-- BLEDNY
Wall-clock sekw : 0.793772 s    |  rown : 0.148730 s
Przyspieszenie  : 5.337
```

## Interpretacja wyników

### Dlaczego wynik jest błędny
`0.199…` zamiast `3.141…` — różnica jest rzędu wielkości, nie zaokrągleń. Mechanizm:
1. **`sum = sum + ...`** to read-modify-write. Dwa wątki czytają tę samą starą wartość, dodają swoje wkłady, nadpisują się — większość aktualizacji jest **gubiona**.
2. **`x`** jest współdzielone — wątek A nadpisuje `x` gdy B już odczytał; obliczenia B używają `x` wątku A.

Wynik nie jest nawet blisko poprawnej wartości — z 10^9 iteracji tylko drobny ułamek dotarł do sumy. Każde uruchomienie daje inną wartość (0.199, 0.356 itp.).

### Dlaczego mimo wszystko „przyspieszenie 6.4"
**To przyspieszenie jest fikcyjne** — kod nie liczy tego samego, co sekwencyjnie. Większość aktualizacji do `sum` nie dociera, więc pętla nie wykonuje faktycznie tej samej pracy. Gdyby kompilator policzył wszystkie N = 10^9 dodawań, czasy byłyby znacznie gorsze (jak w PI3) ze względu na koherencję linii `sum`.

### Dlaczego czas CPU (2.24 s) > wall-clock × T?
Wallclock = 0.127 s × 20 wątków = 2.54 s teoretycznego maksimum. Zmierzony CPU = 2.24 s → ~88% wysycenia. Różnicę pochłania ruch koherencyjny (linia `sum` „lata" między rdzeniami) i oczekiwanie.

## Stopień lokalności dostępu do danych
**Bardzo niski.** `sum` i `x` są w pamięci (adresy współdzielone), więc:
- każdy zapis wątku do `sum` unieważnia linię w pozostałych rdzeniach (MESI: M→I),
- kolejny dostęp innego wątku wymaga ściągnięcia aktualnej linii (cache-to-cache transfer),
- to samo dotyczy `x`.

## Kiedy i dlaczego dostępy do pamięci
- W każdej iteracji — `sum` i `x` są współdzielone, więc ich wartości muszą być widoczne globalnie. Kompilator nie schowa ich w rejestrze (adres jest obserwowalny przez inne wątki).
- Przyczyna: współdzielenie + model spójności OpenMP.

## Unieważnianie linii cache
**Występuje w każdej iteracji.** Linia zawierająca `sum` jest cyklicznie unieważniana w rdzeniach oprócz piszącego. To **nie jest** jednak główna przyczyna „przyspieszenia" — kod po prostu gubi większość pracy.

## Wnioski
Wersja nieużyteczna do obliczeń — wynik jest przypadkowy. Demonstracja mechanizmu race condition. PI3 rozwiązuje problem atomowością.
