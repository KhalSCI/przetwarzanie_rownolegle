# Część 2 — PI1…PI7 — pomiar kosztów współdzielenia danych między wątkami

## Zawartość

| Folder | Wariant | Opis |
|---|---|---|
| `PI1/` | sekwencyjny | baseline, `-O3`, bez OpenMP w pętli |
| `PI2/` | `parallel` + `for` | bez ochrony — demonstracja race condition |
| `PI3/` | `atomic` na sumie | poprawny, ale silna kontencja (num_steps zmniejszone do 10^7) |
| `PI4/` | prywatny akumulator + `atomic` na końcu | ręczna redukcja |
| `PI5/` | `reduction(+:sum)` | idiomatyczny OpenMP |
| `PI6/` | tablica sum częściowych (false sharing) | `volatile` + `flush` |
| `PI7/` | eksperyment wyznaczenia długości linii cache | 2 wątki, pary `(tab[k], tab[k+1])` |

Każdy folder zawiera `pi.c` oraz `README.md` z uzasadnieniem decyzji projektowych, analizą zmiennych, lokalności dostępu, dostępów do pamięci, unieważniania linii cache, faktycznymi pomiarami i obliczonym przyspieszeniem.

## Kompilacja i uruchomienie

```bash
make              # buduje wszystkie 7
make clean
```

Uruchomienie z 20 wątkami (logiczne) lub 10 (fizyczne):
```bash
OMP_NUM_THREADS=20 ./PI5/pi
OMP_NUM_THREADS=10 OMP_PLACES=cores OMP_PROC_BIND=close ./PI5/pi
```

PI7 (wymaga dokładnie 2 wątków na różnych rdzeniach):
```bash
OMP_NUM_THREADS=2 OMP_PLACES=cores OMP_PROC_BIND=spread ./PI7/pi
```

## Sprzęt referencyjny (pomiary faktyczne)
- CPU: **Intel Core i7-12700K** (hybrydowa architektura: 8 rdzeni P + 4 rdzenie E, razem 10 fizycznych / 20 logicznych wg `lscpu`)
- L1d cache line: **64 B** (potwierdzone przez PI7)
- System: Linux, kompilator gcc -O3 -fopenmp
- `num_steps = 10^9` dla wszystkich poza PI3 (gdzie 10^7 — atomic przy 10^9 zajmowałby 40+ s)

## Tabela zbiorcza wyników (PI1–PI6)

Wallclock i przyspieszenie zmierzone, identyczny baseline sekwencyjny w każdym pliku:

| Wersja | Wynik | Wall-clock sekw [s] | Wall-clock rown [s] | CPU rown [s] | Przyspieszenie T=20 | Przyspieszenie T=10 |
|---|---|---|---|---|---|---|
| PI1 | 3.141592653589971 | 0.830 | — | — | 1.00 (baseline) | — |
| PI2 | **BŁĘDNY** (0.199 / 0.356) | 0.809 | 0.127 | 2.24 | 6.39 (fałszywe!) | 5.34 (fałszywe!) |
| PI3 *(num_steps=10^7)* | 3.141592653590 | 0.008 | 0.407 | 7.90 | **0.020** | 0.035 |
| PI4 | 3.141592653590 | 0.832 | 0.128 | 2.32 | **6.52** | 5.59 |
| PI5 | 3.141592653590 | 0.830 | 0.120 | 2.23 | **6.92** | **7.47** |
| PI6 | 3.141592653590 | 0.816 | 13.126 | 259.2 | **0.062** | 0.049 |

## Wnioski

### Porządek od najlepszego do najgorszego przyspieszenia
1. **PI5 (reduction)** — 6.92× (T=20) / 7.47× (T=10). Najczystsze, najszybsze.
2. **PI4 (prywatny akumulator + atomic na końcu)** — 6.52× / 5.59×. Identyczna filozofia, drobna strata na liniowym scalaniu.
3. **PI2 (race condition)** — pozorne 6.39×, ale wynik błędny — **nieporównywalne**.
4. **PI6 (false sharing)** — 0.062× — **16× wolniejsze** niż sekwencyjne.
5. **PI3 (atomic w gorącej pętli)** — 0.020× — **50× wolniejsze** niż sekwencyjne.

### Kluczowe obserwacje
- **Spójność pamięci to dominujący koszt.** PI3 i PI6 mają poprawny wynik, ale przez ruch koherencyjny tracą cały zysk z równoległości i są znacząco wolniejsze od wersji sekwencyjnej.
- **Prywatyzacja danych jest kluczowa.** PI4 i PI5 nie mają gorącej zmiennej współdzielonej, więc wątki nie wpływają na siebie nawzajem przez L1d — stąd przyspieszenie bliskie teoretycznemu sufitowi.
- **Czas CPU vs wall-clock** — dla wersji równoległych stosunek mówi, ile rdzeni realnie pracowało w danej chwili. PI4/PI5 mają stosunek ~18–19 (bliski 20) — prawie wszystkie rdzenie liczą. PI3 ma 19× ale to rdzenie „zajęte" w sprzętowej kolejce koherencji — czas CPU jest wysoki, ale praca nie postępuje.
- **10 wątków > 20 wątków w PI5** — efekt hybrydowej architektury 12700K. Przy 20 wątkach pętla jest limitowana przez wolniejsze rdzenie E (bariera końcowa), przy 10 wątkach wszystkie trafiają na szybkie rdzenie P i HT nie wchodzi w grę.

### Długość linii cache (z PI7)
**64 bajty** — outliery w czasach obliczeń dla par `(tab[k], tab[k+1])` występują co 8 iteracji (8 × 8 B = 64 B). Zgodne z oczekiwaniem dla Intel x86.

## Co zawierają pliki README w podkatalogach
Szczegółowe wyjaśnienia:
- które zmienne są prywatne/współdzielone i dlaczego,
- kiedy i dlaczego realizowane są dostępy do pamięci,
- stopień lokalności dostępu (stosunek łatwe/trudne),
- czy i dlaczego występuje unieważnianie linii cache,
- interpretacja CPU vs wall-clock,
- **faktyczne wyniki pomiarów** + obliczone przyspieszenie + interpretacja.
