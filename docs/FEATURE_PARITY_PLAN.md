# Plan: pełny zestaw funkcji CAD na Androidzie

Właściciel (2026-11-03): aplikacja na Androida ma docelowo umieć to samo, co
desktopowy CAD (punkt odniesienia: zestaw poleceń programów klasy SolidCraft /
Shapr3D / Inventor). Najważniejsza jest **wygoda pracy palcem** — funkcja,
której nie da się wygodnie użyć na telefonie, nie jest jeszcze zrobiona.

Ten plik to plan etapów, nie historia. Stan zrobionych etapów prowadzi
`PROJECT_STATUS.md`; reguły — `CLAUDE.md`.

## Zasady UI/UX dla każdej nowej funkcji

1. **Zaznaczenie podpowiada akcję.** Dotknięcie ściany, krawędzi albo operacji
   pokazuje przy modelu tylko te polecenia, które tam zadziałają (jak „Sketch
   here”, „Fillet/Chamfer”). Żadnej wstążki z 60 ikonami.
2. **Wartość przy geometrii.** Liczby wpisuje się w polu stojącym przy modelu;
   tam, gdzie to ma sens, wartość da się też przeciągnąć uchwytem.
3. **Jedna akcja = jeden krok Cofnij**, a podgląd pokazuje dokładnie to, co
   zostanie zatwierdzone. Odmowa mówi po imieniu, czemu.
4. **Historia jest miejscem edycji.** Każda operacja ma wiersz w History i
   otwiera się z niego w tym samym edytorze, w którym powstała.
5. **Nie rysujemy przycisku, który nie zadziała.**
6. **Cel dotyku ≥ 48 dp**, ikona mówi kształtem, kolor jest drugim nośnikiem.

## Kolejność etapów

Status: ✅ zrobione · 🔜 w realizacji · ⏳ zaplanowane

### Modelowanie części — rdzeń

| # | Etap | Zawartość | Status |
|---|---|---|---|
| 1 | Szkic i podstawy | szkic, Wyciągnięcie (New Body/Add/Cut/Intersect, extent), Obrót New Body, regiony, ściany jako podpory | ✅ |
| 2 | Krawędzie | Zaokrąglenie, Faza prostych krawędzi | ✅ |
| 3 | Wygoda szkicu | Cofnij/Ponów w szkicu, nóż (Trim), przepychanie strzałki do Cut | ✅ |
| 4 | Szyk i Lustro operacji | szyk liniowy (1–2 kierunki), kołowy, lustro operacji Add/Cut względem płaszczyzny bryły | ✅ |
| 5 | Otwór | Otwór prosty, z pogłębieniem walcowym lub stożkowym, na głębokość albo przez wszystko, z dotknięcia ściany | ✅ |
| 5b | Wyciągnięcie „przez wszystko” | extent Through All dla Cut | ✅ |
| 5c | Do ściany i wymiary położenia | extent Do ściany dla Add i Cut; położenie otworu wymiarami od krawędzi | ✅ |
| 6 | **Płaszczyzny konstrukcyjne** | szkic na płaszczyźnie odsuniętej od ściany i pod kątem (✅); przez 3 punkty, od płaszczyzny świata (⏳) | 🔜 |
| 7 | Skorupa i ściany | Skorupa, Wciśnij/wyciągnij ścianę, Odsuń ścianę, Usuń ścianę, Pochylenie | ⏳ |
| 8 | Obrót i bryły | Obrót Add/Cut (✅); prymityw → część CAD: prostopadłościan, walec, sfera, stożek, kapsuła (✅); otwory i operacje na płaskich ścianach części obrotowej (✅); torus, rura (⏳) | 🔜 |
| 9 | Przeciągnięcie i Wyciągnięcie złożone | Sweep wzdłuż ścieżki, Loft brył, Spirala | ⏳ |
| 10 | Kombinacja brył | suma, różnica, część wspólna dwóch brył; Podziel bryłę | ⏳ |
| 11 | Parametry | działania w polach liczbowych (✅); nazwane wartości, Tabela konfiguracji (⏳) | 🔜 |
| 12 | Sprawdź | objętość, pole, masa, środek ciężkości, odległość dwóch punktów (✅); kąt, przekrój (⏳) | 🔜 |
| 13 | Krawędzie 2 | zaokrąglenia krawędzi łukowych i wklęsłych, zmienna faza | ⏳ |
| 14 | Historia 2 | suwak cofania w historii, wyłączanie operacji, zmiana kolejności | ⏳ |
| 15 | Detale części | Gwint, Żebro, Siatka żeber, Wytłoczenie, Występ, Zakładka, Zatrzask, Podparcie | ⏳ |

### Powierzchnie

| # | Etap | Zawartość | Status |
|---|---|---|---|
| S1 | Podstawy | płat, wyciągnięcie, obrót, przejście, przycięcie płaskie, zszycie, pogrubienie | ✅ |
| S2 | Edycja | Odwróć normalną, Rozszyj, Cofnij przycięcie, Wydłuż | ⏳ |
| S3 | Zaawansowane | Przeciągnięcie, Wygładź, Zaokrąglenie i Dopasowanie powierzchni, Konwertuj siatkę | ⏳ |

### Moduły

| # | Etap | Zawartość | Status |
|---|---|---|---|
| M1 | Złożenie | komponenty, złącza, grupa sztywna, unieruchom, położenia | ⏳ |
| M2 | Ruch | sterowanie złączami, powiązanie ruchu, badanie i ślad ruchu, kontakty | ⏳ |
| M3 | Blacha | ścianka, zagięcie, rozwinięcie, konwersja na blachę | ⏳ |
| M4 | Spawanie | profile, narożniki, spoiny | ⏳ |

Każdy etap kończy się testami na hoście, testem na emulatorze (`CI DEVICE`),
wpisem w `PROJECT_STATUS.md` i krótkim raportem dla właściciela.
