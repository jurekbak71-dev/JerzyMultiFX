# Audyt po odsłuchu preview 1

## Co powodowało problemy

Stary wrapper uruchamiał również pomijane procesory, utrzymywał komplet opóźnień w pamięci i dodawał opóźnienia Attack/Atomic/IR do czystego toru. Lista setek parametrów zasłaniała podstawowe czynności. Zagnieżdżony MX utrudniał zrozumienie banków A/B. Import IR nie ustawiał jego miksu, a stary przekaźnik MIDI próbował łączyć się ze starymi portami DSP.

Preview 2 usuwa te przyczyny: pomijanie i zwalnianie nieaktywnych silników poza audio, PDC zależne od aktywnego toru, fade przy zmianie topologii, automatyczne włączenie importowanego IR, osobne strony i bypass Chorus/Attack/Atomic, lista silników MD, Settings oraz nowy most MIDI. Własny DRIVE ma filtr FIR przy oversamplingu 4×. Pełny oryginalny solver Muff jest dostępny obok sześciu lżejszych modeli Legacy; domyślny Green Russian używa Legacy. Ciężkie modele obwodowe nie mają gwarancji pracy realtime na RPi.

## Porównanie z oryginałami

Przejrzano parametry, domyślne wartości i GUI źródłowych MX, Chorus, Attack, Atomic, Memory, Reel i Oil. Chorus zachowuje sześć regulatorów i 14 modeli. Attack ma oddzielne sekcje, oryginalne krzywe, ręczny trigger, expression i Return. MD udostępnia model z listy i jego własne parametry, ze wspólnymi kontrolkami nad nimi. Porównanie próbek Chorus i Atomic z niezależnie uruchomionym oryginalnym kodem jest regresją automatyczną; nie jest to odsłuch wszystkich modeli ani dowód identyczności całego toru analogowego.

## Projekty porównawcze

Przejrzano publiczne opisy architektury i dokumentację, bez pomiaru na ich sprzęcie:

- [MODEP](https://github.com/BlokasLabs/modep) i [Patchbox OS](https://blokas.io/patchbox-os/docs/): oddzielny tor JACK, pedalboard i panel WWW; strojenie bufora zależy od karty. Nie kopiujemy konfiguracji innego interfejsu do HeadRush.
- [MOD Host](https://github.com/moddevices/mod-host) i [MOD UI](https://github.com/moddevices/mod-ui): oddzielenie DSP od panelu, ładowanie potrzebnych efektów i kontrola połączeń.
- [Guitarix](https://github.com/brummer10/guitarix): tor wzmacniacza/kolumny i IR ma znaczenie dla brzmienia przesteru. Sam clipping nie zastępuje kompletnego wzmacniacza z kolumną.
- [Zynthian](https://github.com/zynthian/zynthian-ui) i [Carla](https://github.com/falkTX/Carla): jawne połączenia audio/MIDI, czytelne rozdzielenie konfiguracji od gry.

To przegląd wybranych istotnych projektów, nie wszystkich dostępnych efektów gitarowych. Ich renoma nie jest pomiarem latencji tego programu.

## Latencja i ograniczenia

Clean: 0 próbek dodanych przez DSP. DRIVE FIR: 16; aktywne IR: 128; Attack: 2064; Atomic: 2048. Opóźnienia aktywnych sekcji sumują się. Wyłączenie sekcji usuwa jej opóźnienie. Host VST otrzymuje aktualizację PDC również przy zamkniętym edytorze. Zmiana wyrównania następuje z krótkim wyciszeniem, nie przez mieszanie niewyrównanych sygnałów.

Do powyższego dochodzą bufory JACK/hosta, USB i przetworniki. Dla bassambient zachowujemy 44100 Hz / 128 / 3. Panel umożliwia próbę mniejszego bufora i pokazuje przerwy/xruns; nie zapisuje innej konfiguracji JACK za plecami użytkownika. Attack i Atomic nadal mają dużą latencję wynikającą z oryginalnych algorytmów FFT. Nie obiecujemy bezlatencyjności ani jakości potwierdzonej odsłuchem preview 2. Fizyczny HeadRush, FL Studio, Bluetooth, USB MIDI i LCD są osobnymi punktami odbioru.
