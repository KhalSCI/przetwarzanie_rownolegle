# Notatka — szyna koherencji, unieważnianie linii cache, jak działa `#pragma omp atomic` i dlaczego jest tak wolny

Tłumaczenie od zera z analogią księgowych w biurowcu.

## 1. Cache i po co w ogóle problem istnieje

RAM jest wolny (~100 ns dostęp), rdzeń procesora jest szybki (~0.3 ns na instrukcję). Żeby rdzeń nie czekał 300 cykli na każdą wartość, każdy rdzeń ma własną **podręczną kopię** fragmentów RAM — **L1 cache** (64 KB, ~1 ns).

**Analogia:** wyobraź sobie 10 księgowych (rdzeni) w biurowcu. Główna księga (RAM) stoi w piwnicy, dochodzenie tam zajmuje kwadrans. Każdy księgowy ma **własny notes** (L1 cache) na biurku, do którego przepisuje te strony księgi, nad którymi pracuje. Do notesu zagląda w sekundę.

## 2. Skąd się bierze problem

Jeśli księgowy A i księgowy B obaj mają w swoich notesach przepisaną tę samą stronę księgi — i A coś tam zmieni — to **notes B stał się nieaktualny**, ale B o tym nie wie. Jak zrobi obliczenie ze swojej kopii, dostanie zły wynik.

To samo w CPU: wątek A na rdzeniu 0 zapisuje `sum = 5`, a rdzeń 1 wciąż ma w swoim L1 starą wartość `sum = 3`. **Kopie rozjechały się z rzeczywistością.**

## 3. Szyna koherencji i protokół MESI

Żeby temu zapobiec, procesor ma **szynę koherencji** — fizyczną sieć komunikacyjną (w nowoczesnych Intelach to pierścień/mesh łączący wszystkie rdzenie i ich cache) — po której rdzenie **wysyłają sobie nawzajem komunikaty**: „hej, ja zmieniłem tę linię, unieważnijcie swoje kopie!".

Każda linia cache (64 B) ma w każdym L1 **znacznik stanu** według protokołu **MESI**:

- **M** (Modified) — „mam tę linię, zmieniłem ją, jestem jedynym właścicielem"
- **E** (Exclusive) — „mam ją, nikt inny jej nie ma, nie zmieniałem"
- **S** (Shared) — „mam ją, inni też mogą mieć, nikt nie zmieniał"
- **I** (Invalid) — „moja kopia jest **nieważna**, nie używaj jej"

**Co to znaczy „unieważnić linię u innych"?** Rdzeń, który chce **zapisać** coś do linii, musi najpierw zostać jej **wyłącznym właścicielem**. Robi to tak:

1. Wysyła po szynie komunikat **RFO** (Request For Ownership): „proszę, oddajcie mi linię X".
2. Każdy inny rdzeń, który ma tę linię, zmienia jej stan na **I** (Invalid) — czyli „już tej kopii nie używam".
3. Jeśli ktoś miał ją w stanie M (zmodyfikowaną), musi ją najpierw wysłać naszemu rdzeniowi (żeby nie stracić jego zmian).
4. Dopiero teraz nasz rdzeń ma stan M i może bezpiecznie pisać.

Ta cała wymiana = **kilkadziesiąt do ~100 ns** (30–300 cykli procesora w zależności od tego, czy linię trzeba ściągnąć z L2, L3 albo z L1 innego rdzenia).

**Analogia:** zanim A dopisze coś do swojej kopii strony, musi obdzwonić wszystkich pozostałych księgowych: „macie stronę 42? Wymażcie ją sobie z notesu! Jeśli Ty, C, już coś na niej skreśliłeś — wyślij mi tę skreśloną wersję". Dopiero gdy wszyscy powiedzą „ok", A może pisać.

## 4. Jak działa `#pragma omp atomic sum += ...`

Kompilator zamienia to na **jedną instrukcję asemblerową z prefiksem `LOCK`**, np. `lock cmpxchg` albo `lock xadd`. Na x86 prefiks `LOCK` znaczy: „na czas tej instrukcji **zablokuj linię cache** zawierającą tę zmienną i wymuś jej wyłączność w tym rdzeniu".

Co to znaczy w praktyce dla **jednej** iteracji pętli PI3:

1. Rdzeń chce zrobić `sum += x` na zmiennej, której linia jest aktualnie u innego rdzenia.
2. Wysyła RFO po szynie → wszyscy pozostali ustawiają swoją kopię na **I**.
3. Aktualny właściciel (np. rdzeń 7 z poprzedniej iteracji) wysyła zmodyfikowaną linię do naszego rdzenia.
4. Nasz rdzeń ją dostaje, blokuje ją na czas instrukcji, wykonuje `sum += x`, zwalnia blokadę.
5. **Następna iteracja** — to samo się dzieje, bo w międzyczasie ktoś inny zabrał linię.

Czyli w PI3 przy 20 wątkach **linia `sum` cyrkuluje między 20 rdzeniami, po jednym „obiegu" na iterację**. Każda iteracja = ~50–100 ns koherencji. 10 milionów iteracji × 100 ns = **1 sekunda samego ruchu na szynie**, plus faktyczne wykonanie. Stąd:

```
PI1 sekw.    :  8 ms     (pętla z rejestru, bez koherencji)
PI3 rown.    :  407 ms   ← ~50× wolniej mimo 20 rdzeni
PI3 CPU time :  7.9 s    ← bo 20 rdzeni przez 407 ms SIEDZIAŁO w kolejce RFO
```

**Analogia do `atomic`:** to tak jakby każdy księgowy przed każdą zmianą na stronie 42 musiał zadzwonić do wszystkich 19 pozostałych, poczekać aż oddadzą mu wyłączność, zrobić jedno dopisanie, i zwolnić. A następny w sekundę potem robi to samo dla swojego dopisania. Tysiące razy na sekundę, 20 osób walczących o jedną stronę. **Efektywnie pracuje 1 osoba, a 19 siedzi w kolejce przy telefonie.**

## 5. Dlaczego PI4/PI5 nie mają tego problemu

Bo każdy wątek ma **swoją własną** zmienną `sum_local` (PI4) / kopię redukcyjną (PI5) na swoim stosie / w swoim rejestrze. Linia cache z `sum_local` wątku 0 **w ogóle nie interesuje** rdzenia 5 — różne adresy, różne linie, zero komunikatów na szynie. Wątki pracują **w kompletnej izolacji** przez całą pętlę. Dopiero raz na końcu (T razy razem) sumują się do globalnego `sum`. 20 komunikatów koherencyjnych zamiast 10 milionów → niewidoczne w skali całego obliczenia.

## 6. TL;DR

- **Szyna koherencji** = fizyczna sieć, którą rdzenie mówią sobie „zmieniłem X, zaktualizuj się".
- **Unieważnianie** = zmuszanie innych rdzeni, żeby porzuciły swoje kopie linii cache, bo będą nieaktualne.
- **`atomic`** = instrukcja z prefiksem `LOCK`, która przejmuje wyłączność linii na czas zapisu, przesuwając ją między rdzeniami.
- **Dlaczego tak wolne w PI3**: linia `sum` musi podróżować między wszystkimi rdzeniami w **każdej iteracji**, każdy transfer ~50–100 ns, i tak 10 milionów razy. Praca, którą procesor mógł zrobić w rejestrze w 1 ns/iterację, staje się operacją limitowaną przez szynę koherencji w ~100 ns/iterację.

## Powiązanie z kodami w tym projekcie

| Kod | Co robi ze współdzieloną zmienną | Efekt na koherencję |
|---|---|---|
| PI1 | `sum` w rejestrze, jeden wątek | brak koherencji |
| PI2 | `sum` shared, **bez ochrony** | linie latają, wyścig, wynik błędny |
| PI3 | `sum` shared, `atomic` w każdej iteracji | **10^7 × RFO** → 50× wolniej |
| PI4 | `sum_local` private + 1× `atomic` na końcu | T × RFO → pomijalne |
| PI5 | `reduction(+:sum)` = to samo automatycznie | T × RFO → pomijalne |
| PI6 | `tab[id]` w pamięci, sąsiednie słowa jednej linii | false sharing, linia lata → 100× wolniej od PI4 |
| PI7 | celowo wywołany false sharing jako narzędzie pomiaru | pozwala wyznaczyć linię = 64 B |
