# Przegląd DSP — preview 4

Przegląd kodu, nie ocena odsłuchowa. Wspólny silnik VST/RPi, stałe identyfikatory
parametrów i format presetów. Repozytoria oryginalne pozostają tylko do odczytu.

| Silnik | Wniosek / zmiana |
|---|---|
| Tape Drive | Osobny Warm: asymetryczne miękkie nasycenie z normalizacją wzmocnienia małosygnałowego i domieszką clean. Większy zakres mechaniki, płynny start jej opóźnienia. Hot zachowany. |
| Cloud | Dedykowana sześciostopniowa dyfuzja stereo, dłuższa sieć opóźnień, czasowa skala modulacji, dekorrelacja wyjść. Własny algorytm inspirowany charakterem ambientowych pogłosów; nie kopia ani potwierdzona emulacja Strymon. |
| Plate / Room / Cathedral | Feedback wyliczany z rzeczywistego czasu odczytu, nie rozmiaru przydzielonego bufora. Size nie zmienia już nominalnej kalibracji Decay. Rzeczywisty ogon nadal zależy od tłumienia/filterów. |
| Shimmer | Ta sama korekta Decay. Mocniejszy udział pitchshift, wprowadzony przed współczynnikiem zaniku; bez osobnej pętli omijającej tłumienie. |
| Digital Delay | Wygładzanie zmiany czasu zachowuje stałą czasową dla różnych sample rate, zamiast stałej liczby próbek. Pozostaje interpolowanym delayem ze zmianą wysokości przy zmianie czasu. |
| COMP | Wygładzanie końcowego gainu ma stałą czasową niezależną od sample rate. Zachowano detektor stereo, soft knee i ochronę wyjścia. |
| Memory Delay | Stałe współczynniki przeliczane w prepare. Porównanie z kodem przed optymalizacją: sześć modeli, 44.1/48/96 kHz, dwa kanały, dwa sekundy/model, maksymalna różnica próbek 0 w lokalnym buildzie. |
| Chorus | Zachowano oryginalną strukturę modeli BBD, filtrację i modulację; istniejący test porównuje sygnał z oryginalnym silnikiem. Bez zmiany charakteru na podstawie samego przeglądu. |
| Attack / Atomic | Zachowano pełne silniki, nie zastępowano prostymi demonstratorami. Algorytmiczna latencja odpowiednio 2064/2048 próbek przy 48 kHz nie znika po przyspieszeniu panelu; rdzeń raportuje ją zależnie od aktywnej trasy. |
| MX / Reel / Oil | Zachowano odrębne mechaniki, wielogłowicowość, nieliniowości i ograniczenia sprzężeń; brak uzasadnienia do zmiany wszystkich modeli naraz bez odsłuchu. Przegląd adapterów: natywne wywołania DSP, bez mostu VST. |
| PREAMP / TS / DS / FUZZ | Zachowano charakterystyki i istniejący oversampling DRIVE. Pełny solver wybranych obwodów Muff jest droższy od legacy; nie zmieniono modelu zapisanego przez użytkownika ani nie włączono drogiego modelu domyślnie. |
| IR | Zachowano partycjonowaną konwolucję, resampling i podmianę poza callbackiem. Latencja 128 próbek aktywnego IR jest jawna. |

Poza DSP: poprawki zielonego/czerwonego stanu, MIDI Learn, list/przełączników,
aktualizacji gałek podczas ruchu, obsługi mono/BPM, autostartu i odzyskiwania MIDI.
Autosave działa w oddzielnym wątku, czeka 150 ms ciszy w zmianach, a ciągły ruch
zapisuje nie rzadziej niż co około 750 ms (plus czas I/O). Zamknięcie usługi opróżnia
ostatni stan bez czekania na debounce. Przy nagłym zaniku zasilania końcówka zmian
może pozostać niezapisana. Core i USB bridge restartują także po poprawnym zakończeniu
procesu wskutek restartu JACK. Samo LOAD/import presetu także oznacza stan do
automatycznego zapisu; osobna regresja restartuje rdzeń bez dalszych edycji.

Sprawdzone lokalnie: CTest 3/3 (w tym harmoniczne Warm, gęstość/stereo/reset Cloud,
kalibracja Decay, brak alokacji RT), Python 28/28, Node 9/9, rzeczywiste HTTP/core,
import presetu, snap trybów, trwałość BPM/mono/stanu/map MIDI. Wynik native ARM64
oraz gotowej paczki należy odczytać z końcowego wpisu STATUS.md i runu wydania.
Nie wykonano odsłuchu A/B, pomiaru aliasingu ani fizycznego testu HeadRush/BLE.
