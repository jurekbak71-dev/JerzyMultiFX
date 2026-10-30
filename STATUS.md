# Jerzy MultiFX — status

## Preview 18 — mapa kontrolerów i czyszczenie przypisań MIDI na Pi

- W Ustawieniach RPi jest osobne okno z autorskimi schematami Hotone Ampero Control (cztery przełączniki i pedał ekspresji) oraz WORLDE EASYCONTROL.9. Kliknięcie elementu i naciśnięcie fizycznej kontrolki zapamiętuje wysyłany CC/PC; dla CC można wyszukać i wybrać parametr MultiFX. Układ fizycznych kontrolek zapisywany jest na Pi, a CC→parametr nadal jest częścią każdego presetu. PC pokazuje numer slotu `programN`.
- Przycisk „Wyczyść wszystkie przypisania MIDI” po potwierdzeniu usuwa mapy CC i aktywną ekspresję z bieżącego stanu i wszystkich zapisanych presetów. Sloty PC są odłączane przez zmianę nazw presetów na `programN-bez-PC`, dzięki czemu brzmienia i IR pozostają dostępne. Stan autostartu jest zapisywany przed odpowiedzią. Kalibracja graficznego układu, parowanie Bluetooth oraz połączenia USB MIDI pozostają.
- W tej prostej wersji CC o tym samym numerze z dwóch urządzeń współdzieli funkcję; rdzeń nie rozdziela map według portu/kanału. Zakres pięta–palce i odwrócenie pedału nie mają jeszcze osobnego ustawienia. Schematy są rysowane lokalnie, bez dołączania zdjęć referencyjnych do paczki.
- Lokalnie: 3/3 CTest, 33/33 Python, 15/15 Node oraz test integracyjny rzeczywistego rdzenia/HTTP PASS. [CI 41028005782](https://github.com/jurekbak71-dev/JerzyMultiFX/actions/runs/41028005782) przeszło ARM64; pobrano [paczkę preview 18](https://github.com/jurekbak71-dev/JerzyMultiFX/releases/tag/v0.1.0-preview.18.ci.41028005782.1) i sprawdzono SHA256, ZIP oraz architekturę DEB ARM64. Windows CI było jeszcze w toku przy zapisie tej notatki. Fizyczny Hotone, EASYCONTROL.9, pedał ekspresji i Pi/HeadRush nie zostały przetestowane na tym buildzie.

## Preview 17 — spójne kontrolki DELAY na RPi i VST

- DELAY A i B pokazują wspólny BANK MIX i poziom u góry w każdym silniku, także w podstawowym widoku Memory, Reel i Oil Drum. MIX jest suwakiem; przełączniki Reel ON, głowice, SYNC, STOP, DUB, SOS, FREEZE, TAILS i AUTO AGE są wyraźnymi przełącznikami, a liczbowy SEED nie jest gałką. Memory i Oil zachowują przełączniki i listy wyboru. Nie zmieniono ID, zakresów DSP ani wartości presetów.
- Lokalnie test widoku C++ i panelu Node PASS; [CI 40954739988](https://github.com/jurekbak71-dev/JerzyMultiFX/actions/runs/40954739988) PASS na Windows i ARM64. [Wydanie preview 17](https://github.com/jurekbak71-dev/JerzyMultiFX/releases/tag/v0.1.0-preview.17.ci.40954739988.1) pobrano i sprawdzono SHA256, CRC ZIP oraz architektury PE x64 / DEB ARM64. Fizyczne Pi/HeadRush i FL Studio nadal wymagają testu.

## Preview 16 — zgodność VST z obsługą RPi i MAKEUP Attack/Decay

- Ten sam kod DSP w RPi i VST obejmuje wcześniejsze poprawki brzmienia, bramkę, kompresory i mierniki. Wspólne mapowanie MIDI TIME/FEEDBACK/MIX dla DELAY A/B działa teraz także w procesorze VST; aktywny silnik dostaje CC, a w MX opcjonalnie również drugi silnik. Edytor VST pokazuje trzy przyciski MIDI Learn w DELAY i korzysta z tych samych wskazówek przełączników/list co RPi, w tym trybów Tape Drive. Przełączniki otrzymały czytelny status aktywny/zgaszony.
- Attack/Decay w MultiFX oraz osobne VST mają teraz opcjonalny MAKEUP kompresora −12…+12 dB, domyślnie 0 dB. Dodano go na końcu listy parametrów MultiFX, aby nie zmienić istniejących ID i map MIDI. Parametr jest widoczny w zaawansowanym panelu Attack/Decay. Testy nowego sterowania DSP i migracji stanów są w obu projektach.
- RPi autostart, USB/BLE MIDI i połączenia JACK są funkcjami urządzenia, nie adaptera DAW. VST korzysta z MIDI i audio dostarczanego przez host; nie uruchamia usług Pi.
- [CI 40950484546](https://github.com/jurekbak71-dev/JerzyMultiFX/actions/runs/40950484546) przeszło testy Windows x64 i ARM64; [wydanie preview 16](https://github.com/jurekbak71-dev/JerzyMultiFX/releases/tag/v0.1.0-preview.16.ci.40950484546.1) pobrano i sprawdzono SHA256, CRC ZIP, PE x64 oraz ELF AArch64. Testy fizycznego RPi/HeadRush oraz odsłuch w FL Studio pozostają do wykonania.

## Preview 15 — ujemny MAKEUP kompresorów

- Główny kompresor oraz kompresory wejścia i wyjścia Oil Drum w obu bankach DELAY mają teraz zakres od -24 do +24 dB i 0 dB pośrodku gałki. Zapis projektu w wersji 3 przelicza stare wartości 0..+24 dB z wersji 1/2, zachowując ich faktyczne wzmocnienie i mapowania MIDI. Kompresor optyczny Tape Drive już miał zakres -12..+12 dB w DSP; panel RPi i tekst parametru VST wyświetlają go teraz w dB z właściwym znakiem.
- Testy obejmują migrację starego presetu, etykiety RPi/VST i przetwarzanie dźwięku przez kompresory. Lokalnie: 3/3 CTest, 33/33 Python i 11/11 Node PASS. [CI 40947356503](https://github.com/jurekbak71-dev/JerzyMultiFX/actions/runs/40947356503) PASS na Windows x64 i ARM64; [wydanie preview 15](https://github.com/jurekbak71-dev/JerzyMultiFX/releases/tag/v0.1.0-preview.15.ci.40947356503.1) pobrano i potwierdzono SHA256, CRC ZIP i architektury PE x64 / ELF AArch64. Automatykę hosta VST zapisaną poza projektem trzeba ponownie ustawić, bo jej znormalizowane 0..1 ma teraz zakres -24..+24 dB. Fizyczne Pi/HeadRush nadal wymaga weryfikacji.

## Preview 14 — Attack/Decay: odciążenie toru MONO

- Wspólny silnik Attack/Decay utrzymuje historię wejścia POLY, lecz w MONO nie wykonuje transformacji FFT. Po wybraniu POLY przetwarza pierwszą ramkę przed domieszaniem; przejście ma test ciągłości audio. Fuzz 16× nie liczy się, gdy jest wyłączony; po ponownym włączeniu jego bufor jest zerowany i krótko rozgrzewany przed domieszaniem. Zmiany są w obu produktach MFX oraz osobnym VST A-D.
- Lokalny benchmark x86, 48 kHz/128, 1 sekunda audio, domyślny A-D MONO bez fuzz: ok. 0,177 s CPU przed zmianą i 0,0094 s po zmianie. To pomiar tej maszyny, nie fizycznego Pi. Latencja A-D nadal wynosi 2064 próbki (~43 ms przy 48 kHz); jej zmiana wymaga osobnej przebudowy kompensacji hosta.
- Testy: pełny natywny zestaw CTest i nowa regresja MONO→POLY/fuzz, zachowanie sygnału przy przełączeniu; CI i paczka ARM64 do uzupełnienia po wydaniu.
- [CI RPi 40944200219](https://github.com/jurekbak71-dev/JerzyMultiFX/actions/runs/40944200219) PASS; [preview14](https://github.com/jurekbak71-dev/JerzyMultiFX/releases/tag/v0.1.0-preview.14.ci.40944200219.1) pobrano ponownie, SHA256 wszystkich assetów i CRC ZIP PASS, binarium to ELF AArch64. Lokalnie 3/3 CTest, 33/33 Python, 10/10 Node; cross-build MultiFX Windows VST3 PE32+ x64 PASS. [Pełne CI 40944794541](https://github.com/jurekbak71-dev/JerzyMultiFX/actions/runs/40944794541) PASS: Windows x64 i ARM64. Test na fizycznym RPi/HeadRush nadal niewykonany w tej iteracji.
- Osobne VST A-D: [CI Windows 40944041793](https://github.com/jurekbak71-dev/JerzyAttackDecayVST3/actions/runs/40944041793) PASS (DSP, pluginval, edytor, pakowanie), [preview VST3 x64](https://github.com/jurekbak71-dev/JerzyAttackDecayVST3/releases/tag/v0.2.1-preview.ci.40944041793.1) pobrano ponownie: SHA256 `55c8d432b017d03faa83a01f7bfa49ecbd00285f6b1f127b025f65cdbd79c63`, CRC i PE32+ x64 PASS. Lokalny pełny zestaw źródłowego A-D: 2/2 CTest, łącznie 318 s. Syntetyczny rendering poprzedniej i nowej wersji w stałym MONO, POLY i fuzz: maksymalna różnica próbek 0 w każdym przypadku.

## Preview 13 — wizualne wskaźniki IN/OUT

- Panel RPi wyświetla teraz paski poziomu IN/OUT obok wartości dB, z czerwonym sygnałem blisko 0 dBFS. Korzystają z istniejących próbek maksimum obliczanych przez silnik i nie dodają opóźnienia DSP; aktualizują się tylko podczas zwykłego odświeżenia panelu.
- [CI RPi 40940796427](https://github.com/jurekbak71-dev/JerzyMultiFX/actions/runs/40940796427) PASS; [preview13](https://github.com/jurekbak71-dev/JerzyMultiFX/releases/tag/v0.1.0-preview.13.ci.40940796427.1) zawiera nowy panel i wszystkie poprawki preview12. Wszystkie assety pobrano ponownie: manifest SHA256 i CRC instalatora PASS, binarium TAR to ELF AArch64. Fizyczne Pi/HeadRush nadal niesprawdzone.
- Po zmianach w DSP wykonano także lokalny cross-build MultiFX Windows VST3: PASS, PE32+ x64 DLL. W tej iteracji nie wykonano Windows pluginval dla MultiFX; osobne VST Attack/Decay ma własny Windows CI.
- Stroik: zalecana osobna opcjonalna analiza sygnału w wątku pomocniczym z kopiowaniem próbek przez bezblokujący bufor. Nie został jeszcze wdrożony; stałe obliczanie wysokości dźwięku w callbacku zwiększałoby obciążenie niepotrzebnie.

## Preview 12 — odrębne algorytmy REVERB, Shimmer i etykiety DELAY B

- Plate: krótka kaskada dyfuzorów, gęsty jasny ogon i niezależne wyjściowe odczepy FDN. Room: cztery dyskretne wczesne odbicia 11–47 ms, krótszy/słabiej dyfundowany ogon. Cathedral: dłuższe ciemniejsze linie, sześciostopniowe rozpraszanie i własna topologia odczepów. Cloud pozostaje szeroką kaskadą sześciostopniową; Shimmer ma stereofoniczny pitch-shifter w sprzężeniu i słyszalną bezpośrednią składową przesuniętą.
- Nowy stan Shimmer domyślnie wybiera +12 półtonów. Identyfikatory parametrów i zapisane mapowania MIDI pozostają bez zmian; zapisane presety zachowują własną wartość pitch.
- Etykiety wewnętrznych silników MX w DELAY B wskazują teraz bank `DELAY B` oraz `MX 1/2` w panelu i metadanych, bez zmian kluczy lub wartości.
- Sekcja DRIVE ma bramkę szumów ON, próg i czas zwalniania. Bramka działa przed przesterem, bez lookahead i bez dodanego opóźnienia. Istniejące wskaźniki IN/OUT są wyliczane z próbek audio i odczytywane z atomowych liczników; nie dodają latencji. Stroik wymaga osobnego, opcjonalnego widoku.
- Attack/Decay: analiza załączonego schematu EHX i instrukcji producenta ujawniła rolę krótkiego impulsu monostabilnego przy detekcji szarpnięcia. Test nakładających się nut wykazał podwójny trigger 22–36 ms po pierwszym; wydłużono okres blokady wtórnego impulsu do 45 ms. Test obejmuje nuty 150/300/600 ms po poprzedniej; osobne VST A-D ma analogiczną poprawkę na osobnej gałęzi repozytorium.
- Osobne VST A-D: [szkic PR 1](https://github.com/jurekbak71-dev/JerzyAttackDecayVST3/pull/1), [Windows CI 40941455444](https://github.com/jurekbak71-dev/JerzyAttackDecayVST3/actions/runs/40941455444) PASS (DSP, procesor, pluginval, edytor, pakowanie, bezpośrednia publikacja), [prywatne preview VST3 x64](https://github.com/jurekbak71-dev/JerzyAttackDecayVST3/releases/tag/v0.2.1-preview.ci.40941455444.1). Pobrany ZIP: SHA256 `acce85372ed298335ad8e592812b6ce6fe079cc09a90385c1fd901d7fec111f8`, CRC PASS, moduł PE32+ x64. Nie jest to pełna rekonstrukcja brzmienia starego EHX ani walidacja w FL Studio.
- Walidacja sygnałowa: osobne odpowiedzi impulsowe pięciu silników, pasmo oktawy Shimmer przy 440 Hz, stabilność i reset pamięci. Testy lokalne oraz paczka ARM64: patrz najnowszy wpis po zakończeniu CI. Fizyczny RPi/HeadRush pozostaje nieweryfikowany.
- CI RPi run [40939946920](https://github.com/jurekbak71-dev/JerzyMultiFX/actions/runs/40939946920) PASS: natywny ARM64 build, CTest, Python, Node, JACK dummy/audio/MIDI oraz publikacja [preview12](https://github.com/jurekbak71-dev/JerzyMultiFX/releases/tag/v0.1.0-preview.12.ci.40939946920.1). Pobranie wszystkich assetów i kontrola manifestu SHA256 PASS; ZIP instalatora przechodzi CRC; binarium w TAR rozpoznane jako ELF AArch64. Lokalnie 3/3 CTest, 33/33 Python, 10/10 Node. Test na fizycznym Pi/HeadRush nadal niewykonany.
- Źródła projektu DSP: [Valhalla — Shimmer](https://valhalladsp.com/2010/05/11/enolanois-shimmer-sound-how-it-is-made/), [Valhalla — charakter algorytmów](https://valhalladsp.com/2014/01/18/naming-reverb-algorithms/), [Valhalla — tryby VintageVerb](https://valhalladsp.com/2023/02/10/valhallavintageverb-the-modes/). To opis zasad akustycznych; implementacja jest własna.

**0.1.0-preview.1 — preview; walidacja sprzętowa nadal wymagana.**

## Rzeczywiste paczki i publikacja — 27.10.2026

- Gotowy prerelease: https://github.com/jurekbak71-dev/JerzyMultiFX/releases/tag/v0.1.0-preview.1.ci.40539397637.1
- Zielony przebieg obu platform: https://github.com/jurekbak71-dev/JerzyMultiFX/actions/runs/40539397637
- Źródło binariów: commit `94950a3b94c237b77c3e29244b0986265b4a8b69`, gałąź `codex/multifx-initial-preview`; zmiany nie są jeszcze scalone do main.
- Windows: rzeczywisty VST3 PE32+ x64 z natywnego MSVC, ZIP 1855161 bajtów. ARM64: rzeczywisty ELF AArch64 z natywnego runnera, TAR.GZ 871333 bajty.
- Oba pliki pobrano ponownie z Release: zewnętrzne i wewnętrzne SHA256, architektury, wykazy plików, źródła i wykluczenie prywatnych materiałów — PASS.
- Release zawiera instrukcje instalacji, wykazy plików i osobne manifesty SHA256 dla obu platform. Repozytorium pozostaje prywatne.

## Migracja Actions → Releases

Każdy job zachowuje kompilację i testy, a następnie bezpośrednio publikuje swoją paczkę. Nie ma transferu binariów między zadaniami przez Actions Artifacts. Numer bazowy pochodzi z VERSION; tag rozwojowy zawiera run ID i attempt, zawsze jako prerelease.

Publikacja ma `contents: write`; PR tylko `contents: read`, bez publikacji. Skuteczne utworzenie i przesłanie plików przez standardowy GITHUB_TOKEN potwierdzono w rzeczywistym CI. Pomocnicza diagnostyka jest domyślnie wyłączona, opcjonalna i ma retencję 1 dzień.

Jawne listy plików oraz `.gitattributes export-ignore` wykluczają prywatne prompty, referencje, konfiguracje, klucze, logi, cache i pliki pośrednie. Pakiety zawierają odpowiadające źródła produktu i niezbędną część SDK, bez zbędnych narzędzi. Przetestowano też faktyczne archiwum źródeł generowane przez GitHub po wprowadzeniu wykluczeń. Nie usunięto istniejących wydań, artefaktów ani historii; publisher odmawia nadpisywania assetów. Szczegóły: [RELEASE_PIPELINE_PL.md](docs/RELEASE_PIPELINE_PL.md).

## Wykonane sprawdzenia

- Natywne CI Windows: build MSVC, CTest z pełnym walidatorem VST3 (47/47), rzeczywisty test edytora obejmujący trzy cykle create/close/unload, ponowna walidacja gotowego bundle podczas pakowania — PASS.
- Natywne CI ARM64: build, CTest DSP/stanu/RT, testy Python, HTTP/control i rzeczywisty sygnał JACK dummy z MIDI Learn — PASS. To nie jest test fizycznego interfejsu audio.
- Lokalne testy Python: 14/14 PASS, w tym wyścig tworzenia Release, odmowa nadpisania i filtrowanie prywatnych danych. Dwa dodatkowe testy transakcji dodano po opublikowanym buildzie; CI źródłowego SHA wykonywało 12 testów Python.
- actionlint obu workflow — PASS.
- Lokalne Windows/Wine: walidator 47/47 i edytor — PASS; ARM64/QEMU DSP i HTTP — PASS.
- Panel w Chromium: logowanie, pięć bloków, zmiana parametrów, przeciąganie kolejności i widok 844×390 — PASS.
- Minimalne źródła z paczki rozpakowano i ponownie zbudowano do Windows VST3 — PASS.

## Zakres implementacji

Rozpakowano 26 referencji; materiały służą analizie i nie są redystrybuowane. Wspólny core C++20 zawiera pięć przestawialnych bloków, źródłowe silniki DSP, A/B delay, FDN reverb i konwolucję IR. Stan z CRC obejmuje osadzone IR i mapy CC; 919 parametrów. Windows ma edytor GDI, automatyzację i MIDI Learn; RPi ma host JACK, panel HTTP z tokenem, BLE GATT/MIDI, logikę LCD oraz skrypty instalacji/rollback.

Pochodzenie kopii silników i SHA opisuje docs/source-manifest.json. Osobne repozytorium JerzyAttackDecayVST3 zostało zaktualizowane na wyraźną prośbę użytkownika; pozostałe repozytoria referencyjne nie były modyfikowane.

## Naprawione regresje

- Ujednolicono statyczny CRT SDK/projektu dla MSVC.
- Pełny walidator przeniesiono z POST_BUILD do CTest: MSBuild błędnie interpretował jego diagnostykę odrzucanych sample rate jako błąd mimo kodu 0. Zachowano asercje i wszystkie kontrole; SDK nie zmieniano.
- Impuls całego łańcucha ujawnił opóźnienia AttackDecay 2064 + Atomic 2048 + IR 128. PDC i bypass wyrównano do **4240 próbek (88,3 ms przy 48 kHz)**; test rzeczywistego impulsu PASS.
- Znormalizowano sprzężenie Shimmer i wygładzono gain CPD.

## Niewykonane testy i ograniczenia preview

Fizyczny Raspberry Pi/USB audio, FL Studio, radio BLE/Bluefy/iOS oraz LCD: **AWAITING HARDWARE VALIDATION**. Nie deklarujemy stabilności sprzętowej ani małej latencji do gry na żywo. Brakuje kreatora ALSA/AP w panelu i zewnętrznego wejścia Return/sidechain AttackDecay. Niektóre źródła mają nierozstrzygnięte noty licencyjne; publiczna dystrybucja wymaga ich uporządkowania. Obecne wydanie pozostaje prywatne.

## Prosty instalator dla użytkownika — 27.10.2026

Dodano `scripts/instaluj-rpi.sh` oraz `INSTALACJA_RPI_PROSTO.txt`. Gotowy plik `dist/JerzyMultiFX-RPi-PROSTA-INSTALACJA.zip` zawiera instalator po polsku, krótką instrukcję, SHA256 i niezmienioną, pobraną wcześniej z Release natywną paczkę ARM64. Nie wymaga logowania do GitHuba ani kompilacji. Instalator sprawdza ARM64/Trixie i integralność paczki, pokazuje wyłącznie karty z wejściem i wyjściem, następnie wywołuje istniejącą instalację z backupem i autostartem. Nie formatuje nośników.

Weryfikacja: składnia Bash, `--help`, test wyboru zgodnych wejść/wyjść, integralność ZIP oraz zgodność osadzonego programu z pobraną paczką — PASS. Instalacja na fizycznym Raspberry Pi pozostaje niesprawdzona. Próba dodania dwóch nowych plików do istniejącego Release została odrzucona przez GitHub HTTP 401; pliki są dostępne lokalnie do pobrania z rozmowy. Nie nadpisano ani nie usunięto dotychczasowych assetów.

## Aktualizacja istniejącego bassambient / HeadRush

Na podstawie wyników użytkownika: Debian 13 ARM64, użytkownik jerzy, działający domyślny JACK 44100 Hz / 128 / 3, HeadRush Core capture/playback 1–2, silnik JerzyAudioPlatform i JerzyBluetoothMIDI. Przygotowano `scripts/aktualizuj-headrush.py` i samowystarczalny `dist/JerzyMultiFX-AKTUALIZUJ.py` z osadzoną, wcześniej zweryfikowaną paczką ARM64. Instalacja w katalogu użytkownika: dwa nowe user services, bez zastępowania systemowego JACK ani zmian sieci. Zachowuje stare pliki/presety i przekaźnik MIDI; zatrzymuje i wyłącza autostart wyłącznie dwóch znanych usług użytkownika starego silnika/panelu. Zapisuje ich stan i skrypt COFNIJ.sh; przy błędzie aktywacji automatycznie próbuje przywrócić poprzedni program. Weryfikuje dostępność portów i gotowość HTTP/audio.

Testy: 17 Python PASS; w tym odrzucenie uszkodzonego pliku, traversal i symlinków; ekstrakcja rzeczywistej natywnej paczki przez nowy instalator PASS, wygenerowany plik kompiluje się i --help działa. Nie wykonano zdalnej instalacji na urządzeniu użytkownika; jej rezultat pozostaje niepotwierdzony. Schemat LCD wymaga identyfikacji modelu/pinów — nie zakładamy napięcia ani zgodności GPIO na podstawie samego słowa wyświetlacz.

## LCD — identyfikacja z materiałów

Obejrzano wszystkie cztery zdjęcia LCD: QAPASS 1602A, etykieta BotiLand LCD-03336, 16 pinów bez I²C; pinout nadrukowany na froncie. Dodano docs/hardware/LCD_1602A_POLACZENIE.md i kopię tekstową do pobrania w dist. Schemat obejmuje sześć sygnałów przez jednokierunkowy 74AHCT245 (5 V, DIR HIGH, /OE LOW), RW LOW, kontrast 10 kΩ, ograniczenie prądu podświetlenia i odsprzęganie. GPIO BCM17/18/27/22/23/24. Nie potwierdzono fizycznych progów wejściowych kontrolera pod zalewą ani testu modułu. Z tego powodu schemat nie zakłada bezpośredniego sterowania 3,3 V bez bufora. Sterownik LCD nie jest jeszcze automatycznie uruchamiany przez instalator audio.

## Naprawa dostarczania instalatora

Linki do plików lokalnych z rozmowy okazały się niedostępne dla użytkownika. Bezpośredni upload z chmury zwracał 401 mimo działającego odczytu API i git push. Dodano workflow `installer.yml`, który używa własnego GITHUB_TOKEN i bez Artifacts publikuje samowystarczalny instalator ZIP oraz oddzielne SHA256 i wykaz plików do istniejącego prywatnego prerelease. Nie nadpisuje wcześniejszych assetów.

Run https://github.com/jurekbak71-dev/JerzyMultiFX/actions/runs/40550934315 — SUCCESS: 17 testów, pobranie natywnej paczki, weryfikacja jej SHA/architektury i publikacja instalatora. Faktycznie pobrano opublikowany ZIP ponownie, sprawdzono SHA256, wykaz i składnię osadzonego skryptu — PASS. Anulowano wyłącznie zbędny nowy pełny rebuild 40550933952, bez usuwania historii; binaria DSP nie zmieniły się.

Działający adres instalatora: https://github.com/jurekbak71-dev/JerzyMultiFX/releases/download/v0.1.0-preview.1.ci.40539397637.1/JerzyMultiFX-INSTALATOR.zip . Pobranie w przeglądarce wymaga zalogowania do konta z dostępem do prywatnego repozytorium. Instalacja na urządzeniu użytkownika nadal czeka na wykonanie.

## Preview 2 — prace po uwagach z odsłuchu i GUI

Właściciel potwierdził uruchomienie preview 1 na fizycznym bassambient/HeadRush i panelu HTTP, następnie zgłosił nieakceptowalne brzmienie/ergonomię, bypass i IR. Sam fakt uruchomienia nie oznacza potwierdzenia jakości. START autoryzuje poprawki obu platform i przygotowanie nowych rzeczywistych paczek.

W implementacji: dynamiczna latencja zależna od aktywnych efektów, pomijanie wyłączonych silników, opóźnienia tworzone i zwalniane poza audio, oddzielne panele Chorus/Attack/Atomic, MD z listami rodzin/modeli i parametrami wybranego modelu, Settings, most do istniejącej usługi Bluetooth i wybór MIDI/portów audio. Uzupełniany jest oryginalny solver 14 modeli Muff oraz sześć modeli Legacy; Attack otrzymuje Return i expression. Wewnętrzny ABI modułów jest w wersji 2, stan w wersji 2 z migracją starych map CC. Stabilne ID parametrów/VST zachowane; nowe dopisywane na końcu.

Dotąd: lokalne testy dotychczasowe oraz pierwsza regresja rzeczywistych sygnałów PASS (clean0/IR128/Attack2064/Atomic2048, porównanie próbek Chorus/Atomic z oryginalnym kodem). Pierwszy nowy edytor Windows testowany jako prawdziwa DLL pod Wine: wybór 14 modeli Chorus, odrębne strony, wybór dziewięciu modeli MX i ich zmiana — PASS. Dalsze rozszerzenia Return/expression/stanu i pełne aktualne paczki nadal wymagają ponownego sprawdzenia. Nie instalowano zmian zdalnie na sprzęcie użytkownika.

### Preview 2 — wykonana weryfikacja lokalna

953 parametry; czysty start z pominiętymi efektami. Maksymalne opóźnienie aktywnego łańcucha 4256 próbek obejmuje nowy FIR DRIVE 16; nie jest narzucane torowi clean. Sekcje Chorus/Attack/Atomic mają osobny bypass, MD ma jedną listę 13 silników na bank i pełne parametry zaawansowane. IR import włącza blok/miks. Return jest dostępny w JACK i jako dodatkowy bus VST. Settings obejmuje bufor JACK, routing, ochronę wyjścia i diagnostykę. Most MIDI nie odwołuje się do starych portów JerzyAudioPlatform.

- Python: 24 testy PASS, w tym kontrola brokera/UID, protokół BLE i rzeczywisty skrypt przywracania poprzednich plików w symulowanej instalacji. Nie jest to test fizycznej aktualizacji.
- CTest Linux: 2/2 PASS — stan/RT i rzeczywiste sygnały; clean 0, IR 128, Attack 2064, Atomic 2048; oryginalny Chorus/Atomic porównane próbka po próbce; zero alokacji na wątku audio.
- ARM64 cross-build i te same dwa programy testowe pod QEMU: PASS. Natywny build ARM64 w CI będzie osobną kontrolą.
- Rzeczywisty JACK dummy: audio, gain, USB CC Learn, transport Unix → natywny port MIDI JACK → parametr DSP, zmiana bufora 128/256 i Return routing — PASS; lokalny przebieg bez xrun. Nie jest to pomiar HeadRush.
- HTTP/core: autoryzacja, zapis/odczyt presetu, resampling/import/osadzenie IR oraz parametry wybranego modelu — PASS.
- Chromium: ukrycie logowania po połączeniu, 5 bloków, Chorus 14 modeli/6 regulatorów, odrębne Attack/Atomic, trigger, MD 13 silników i strona Brig bez parametrów innych modeli, Settings i Bluetooth/USB MIDI, szerokość 390 px — PASS.
- Prawdziwa DLL VST3 pod Wine: audio clean, import IR zmienia sygnał, bypass IR przywraca dry, aktualizacja PDC na głównym wątku przy zamkniętym GUI i wejście Return — PASS. Pełny edytor i walidacja poprawnej paczki są sprawdzane oddzielnie przed publikacją.

Benchmark lokalny x86_64, 48 kHz/128, po 1 sekundzie audio: clean 0,0031 s CPU; TS 0,0130; Chorus 0,0104; Attack 0,1404; Atomic 0,0445; MX Volante 0,0389; IR 16384 0,0262. Pełny solver Muff Supa 0,3636 s CPU i dlatego nie jest domyślny. Szczyt RSS procesu w kolejnych profilach około 7,4–12,3 MiB. To mikrobenchmark tego komputera, nie gwarancja realtime ani pomiar RPi; wyniki natywnych runnerów są dołączane do VALIDATION.json.

Instalator RPi zawiera teraz plik `ZAINSTALUJ-NA-RASPBERRY.cmd` do dwukliku na Windows. Obsługuje aktualizację istniejącego preview 1, zachowuje presety/IR/mapy CC/klucz, Wi-Fi i konfigurację JACK oraz zapisuje działające cofnięcie. Startuje z czystym brzmieniem zamiast odtwarzać nieakceptowany stan preview 1. Instalator VST tworzy kopię poprzedniego bundle przed podmianą. Nie zainstalowano preview 2 na sprzęcie użytkownika.

Starsze akapity opisują historyczny preview 1; jego ograniczenia 919 parametrów, stałej latencji i braku Return nie opisują preview 2. Fizyczny odsłuch, FL Studio, radio BLE/telefon PAN/USB MIDI i LCD preview 2 nadal czekają na sprawdzenie. Przygotowanie nowych plików do pobrania nie oznacza ich pozytywnej oceny muzycznej przez właściciela.

Dodatkowe końcowe kontrole: poprawiono indeksowanie modeli MX wykryte przez test prawdziwego edytora. Pełny edytor DLL (trzy cykle otwarcia/zamknięcia, zmiany parametrów, MIDI Learn, kolejność, skalowanie, 14 modeli Chorus, wybór Brig z listy MD) — PASS. Poprawna struktura paczki VST3 i pełny walidator SDK pod Wine: **47/47 PASS**. Własne sprawdzenie obu architektur, manifestów SHA256, dołączonych źródeł i allowlisty prywatności — PASS. Samowystarczalny ZIP RPi zawiera trzy pliki, w tym skrypt dwukliku. Publikacja natywnych paczek CI pozostaje następnym etapem.

CI ujawniło dwie różnice względem lokalnej konfiguracji: limit zagnieżdżeń kontekstów testu w Pythonie runnera (zastąpiono ExitStack) i brak dowiązania nowego programu MIDI w katalogu używanym przez test integracyjny (dodano). Nie usunięto ani nie pominięto testów. Dodatkowo edytor VST przelicza listy oryginalnych parametrów według liczby pozycji zamiast błędnego źródłowego znacznika integer; test wyboru podziału Oil chroni tę poprawkę. Początkowe skalowanie dostosowuje również listy do ekranu.

## Preview 2 — faktycznie opublikowane paczki

Źródłowy commit **ecf68e066e03ed97a9fe82bc6704a795721b6939**. Pełny run **SUCCESS** na obu platformach: https://github.com/jurekbak71-dev/JerzyMultiFX/actions/runs/40566730911 . Windows kompilowany natywnie przez MSVC x64; ARM64 natywnie na ubuntu-24.04-arm. Oba zadania publikowały bezpośrednio do tego samego prywatnego prerelease, bez przesyłania wyników przez Actions Artifacts. `contents: write` zadziałało; opcjonalne diagnostyczne Artifacts mają retencję 1 dzień i domyślnie są wyłączone.

Release: https://github.com/jurekbak71-dev/JerzyMultiFX/releases/tag/v0.1.0-preview.2.ci.40566730911.1

- Windows VST3: https://github.com/jurekbak71-dev/JerzyMultiFX/releases/download/v0.1.0-preview.2.ci.40566730911.1/JerzyMultiFX-VST3-Windows-x64-v0.1.0-preview.2.zip — rozpakować i dwuklik `ZAINSTALUJ-VST3.cmd`.
- Aktualizacja istniejącego bassambient: https://github.com/jurekbak71-dev/JerzyMultiFX/releases/download/v0.1.0-preview.2.ci.40566730911.1/JerzyMultiFX-INSTALATOR.zip — rozpakować i dwuklik `ZAINSTALUJ-NA-RASPBERRY.cmd` na Windows; włączyć Raspberry i HeadRush, podawać hasło jerzy.
- Oddzielny TAR.GZ ARM64, instrukcje, wykazy plików i SHA256 obu platform również są w Release.

Natywne CI Windows: CTest DSP/sygnałów + walidator SDK, test rzeczywistego edytora oraz rzeczywistego sygnału DLL/IR/bypass/PDC bez GUI/Return, benchmark i składnia obu instalatorów — PASS. Natywne CI ARM64: CTest 2/2, Python 24 testy, HTTP/IR/presety, JACK dummy/audio/USB-CC/transport BLE-MIDI bez radia/zmiana bufora/Return routing, benchmark, prywatność paczek i osadzony instalator — PASS; w tym przebiegu JACK dummy 0 xrun.

Po publikacji rzeczywiście pobrano oba archiwa binarne, ZIP instalatora i manifesty. Sprawdzono zewnętrzne/wewnętrzne SHA256, ELF64 AArch64 obu programów RPi, PE32+ x64 VST, źródłowy commit i zgodność pakietu ARM z payloadem samowystarczalnego instalatora — PASS. Weryfikacja dotyczy gotowych assetów, nie jedynie lokalnych plików. Dwa zastąpione runy 40566264195 i 40566463081 anulowano; nie usuwano wydań, assetów, artefaktów ani historii. Repozytoria źródłowe nie były edytowane.

Preview 2 nie zostało zdalnie zainstalowane na fizycznym bassambient. Ocena brzmienia/ergonomii przez właściciela, FL Studio, pomiary HeadRush roundtrip i radia Bluetooth nadal wymagają odbioru sprzętowego. Latencja FFT Attack/Atomic pozostaje opisana w audycie; testy programowe nie stanowią obietnicy zerowego opóźnienia całego urządzenia.

## Preview 3 — GUI i obsługa bez klucza

Zmiany: drewniane boki, stal/aluminium/ebonit, przestrzenne gałki, hebelki i lampki. Wskaźniki IN/OUT korzystają z rzeczywistych szczytów silnika i mają niezależną bezwładność; 0 VU odpowiada -18 dBFS, to skala peak, nie certyfikowany pomiar RMS. Przewijane regulatory i natywne listy nie wchodzą w stały pas TRIGGER/bypass/opisów. Oryginalny układ Chorus 3×2, Atomic 2×2 i Attack cztery pomocnicze + cztery główne regulatory zachowano na podstawie źródłowych edytorów (SHA w source-manifest).

Panel RPi otwiera się automatycznie, bez klucza. Wejście na stronę ustawia sesyjne cookie HttpOnly/SameSite=Strict; frontend łączy się i ponawia próbę podczas startu audio. Oznacza to dostęp do sterowania dla urządzeń, które mogą otworzyć panel w lokalnej sieci. Pozostawiono sprawdzanie Origin/Sec-Fetch-Site i uwierzytelnienie Bearer dla klientów API. Nie zmieniano sieci ani widoczności repozytorium.

MIDI Learn jest jawnie dostępne przy wszystkich webowych przełącznikach ON/OFF, również PREAMP, TAPE DRIVE, DRIVE, COMP, osobnych sekcjach, bankach, blokach oraz master bypass. W VST służy do tego prawy przycisk na regulatorze/przełączniku, także na blokach i master. Kontroler powinien wysyłać naprzemiennie CC 0/127 dla trwałego OFF/ON. Mapy zapisują się w stanie.

Instalator HeadRush włącza user services i linger, kopiuje cały last-state.jmfx (zamiast resetować brzmienie) oraz zachowuje backup/rollback. Silnik odczytuje ostatnie brzmienie przy starcie. Autostart systemowego wariantu RPi pozostaje przez istniejące systemd enable. Nie wykonano instalacji ani restartu fizycznego Pi.

Wykonano lokalnie: CTest DSP/sygnałów 2/2, Python 24 testy, HTTP/core z rzeczywistym cookie bez podawania klucza i odmową żądań cross-origin, Node 3 testy automatycznego otwarcia/ponawiania/MIDI Learn — PASS. Końcowy cross-build Windows x64, rzeczywisty edytor DLL pod Wine (3 cykle, zrzuty Chorus/Attack/Atomic/Oil, niezależny ruch wskazówek, MIDI Learn), rzeczywisty sygnał DLL/IR/bypass/PDC/Return oraz pełny walidator SDK 47/47 — PASS. Rozszerzony test HTTP przełącza przez MIDI CC każdy parametr bypass oraz restartuje rdzeń i porównuje pełny stan z mapami MIDI — PASS. Przegląd zrzutów wykrył i usunął kolizję list Oil na gałkach; test sprawdza pozycję listy. Natywne CI i finalna publikacja tej rewizji nadal oczekują na wykonanie. Dotychczasowe wydania są zachowane. Repozytoria źródłowe nie są modyfikowane.

### Test renderowania na małym pulpicie runnera

Natywny job Windows rewizji 59f0c17 zatrzymał się w kroku testu edytora. Lokalny test odczytywał piksele okna z ekranu; część okna może znajdować się poza pulpitem runnera. Renderowanie udostępniono także przez standardowy WM_PRINTCLIENT, a test wskazówek oraz zrzuty korzystają teraz z bitmapy w pamięci, z zachowaniem wszystkich asercji. Dodano dokładny komunikat błędu edytora do adnotacji CI, ponieważ pobranie pełnego logu z hosta Actions zostało odrzucone przez politykę sieci środowiska. Nowy test sprawdzany również przy pulpicie 800×600. Nowa natywna walidacja nadal jest wymagana przed publikacją VST.

## Preview 3 — końcowe paczki opublikowane i ponownie pobrane

Źródłowy commit **50843d24962e946fdd89749af5e5ee12dbb94a2e**. Natywny przebieg obu platform **SUCCESS**: https://github.com/jurekbak71-dev/JerzyMultiFX/actions/runs/40578180112 . Release: https://github.com/jurekbak71-dev/JerzyMultiFX/releases/tag/v0.1.0-preview.3.ci.40578180112.1 . Wcześniejsze akapity o oczekującej walidacji opisują etapy prac, nie końcowy wynik.

- Windows x64/MSVC: CTest DSP/sygnałów i pełny walidator SDK **47/47**, rzeczywisty edytor (3 cykle, parametry, bypass, MIDI Learn, kolejność, skalowanie, niezależne wskazówki IN/OUT i położenie list Oil), rzeczywisty sygnał DLL/IR/bypass/PDC/Return, benchmark oraz składnia instalatorów — PASS. Render testu przez WM_PRINTCLIENT usunął zależność od wielkości pulpitu, bez usuwania asercji. Ostatni podpis zaawansowanego delay przeniesiono do stałego pasa.
- Natywny ARM64: CTest 2/2, Python 24 testy, Node 3 testy panelu/MIDI Learn, HTTP/core z automatycznym cookie, wszystkie przełączniki bypass przez MIDI CC oraz restart i odtworzenie pełnego stanu, JACK dummy/audio/MIDI/Return, benchmark i kontrola paczek — PASS.
- Po publikacji pobrano z tego Release oba archiwa binarne, samowystarczalny instalator RPi, instrukcje i manifesty. Zewnętrzne i wewnętrzne SHA256, PE32+ x64/ELF64 AArch64, źródłowy SHA, prywatność/dołączone źródła, brak formularza klucza w gotowym panelu, kod autostartu i zgodność osadzonej paczki instalatora z opublikowanym TAR.GZ — PASS.

Windows: `JerzyMultiFX-VST3-Windows-x64-v0.1.0-preview.3.zip` → rozpakować → `ZAINSTALUJ-VST3.cmd` przy zamkniętym DAW. RPi bassambient/HeadRush: `JerzyMultiFX-INSTALATOR.zip` → rozpakować → `ZAINSTALUJ-NA-RASPBERRY.cmd`. Panel otwiera się bez wpisywania klucza. Instalator włącza autostart i zachowuje ostatnie brzmienie/mapy MIDI. Przy każdym ON/OFF, również PREAMP/TAPE DRIVE/DRIVE/COMP, jest MIDI Learn; w VST prawy przycisk.

Nie instalowano preview 3 zdalnie na fizycznym Pi. Restart/odsłuch HeadRush, radio Bluetooth i FL Studio nadal wymagają potwierdzenia na sprzęcie właściciela. Repozytoria źródłowe pozostają niezmienione. Zastąpione przebiegi anulowano bez usuwania historii ani istniejących assetów; końcowy przebieg oraz wcześniejsza rewizja 543493d zakończyły się sukcesem na obu platformach.

## RPi — poprawki robocze po uwagach 28.10.2026, BEZ KOMPILACJI

Właściciel potwierdził dobre brzmienie VST i zgłosił problemy RPi: audio, reakcje regulatorów, mylące statusy, niedziałające USB/BT MIDI. Wyraźnie wstrzymał kompilację. Poprawki są lokalne; źródła VST i wspólne algorytmy DSP niezmienione. Zakres i analiza MODEP/Patchbox OS: [RPI_PARITY_WORKING_PL.md](docs/RPI_PARITY_WORKING_PL.md).

W kodzie: ON/OFF zielony/czerwony z jednoznacznym statusem; live zmiany gałek zamiast czekania na koniec ruchu; odświeżanie statusów MIDI bez utraty fokusu/przebudowy panelu; ochrona przed starym STATE i błędem zapisu; listy/przełączniki dla dyskretnych opcji; import presetu VST; osobny wątek autosave; poprawki restartu USB i automatyczne odzyskiwanie radia BT wraz z backup/rollback; RT w dedykowanym wariancie systemd, bez przestawiania istniejącego JACK HeadRush.

Wykonano interpretowane regresje: Node 9/9 i Python 28/28 PASS. C++ i zmienione unity nie zostały zbudowane/uruchomione, porównania brzmienia i wydajności nie wykonano. Nie uruchomiono kompilacji/Actions, nie opublikowano paczki, nie zmieniono instalacji na Pi. Ostatnim opublikowanym wydaniem pozostaje preview 3 z poprzedniego rozdziału; nie zawiera tych poprawek. Weryfikacja dodanego BPM/transportu JACK i wejścia Mono/Stereo względem DAW, pomiar parytetu sygnału i fizyczny odbiór MIDI/audio pozostają do dalszego etapu, a nie jako deklaracja sukcesu.

### Dalsze uwagi właściciela — TD Warm/mechanika i Cloud, BEZ KOMPILACJI

Na nowe zlecenie zmieniono wspólny DSP (tym samym przyszły VST także obejmie tę
rewizję): osobna miękka charakterystyka Warm z normalizacją małosygnałową,
większy zakres wow/flutter i płynne centrum opóźnienia TD, dedykowana sześciostopniowa
dyfuzja stereo Cloud, jego RT60/modulacja i wyjścia. Hot oraz pozostałe algorytmy
reverb zachowują swoje tory audio. Szczegóły i ograniczenia:
`docs/RPI_PARITY_WORKING_PL.md`. Brak kopiowania algorytmów Strymon lub deklaracji
zgodności z ich sprzętem. Dodano nieuruchomione regresje sygnałowe 44.1/48/96 kHz.
Wykonano wyłącznie statyczny przegląd i `git diff --check` PASS. Nie kompilowano,
nie uruchamiano CTest/CI, nie publikowano ani nie instalowano. Brzmienie, aliasing,
wydajność Pi i pomiary ogona nie są jeszcze potwierdzone.

## Preview 4 — przygotowanie instalatora RPi (kompilacja odblokowana)

Właściciel zlecił kompilację i instalator RPi, co zastępuje wcześniejszy zakaz
budowania. Zebrano poprzednie zmiany panelu/MIDI/autostartu i DSP. Dalszy przegląd
oraz poprawki: `docs/DSP_REVIEW_PREVIEW4_PL.md`. VERSION 0.1.0-preview.4.
Lokalnie build x86_64 i CTest 3/3 PASS, Node 9/9 PASS, Python 28/28 PASS,
HTTP/core PASS z nowymi BPM/mono/importem presetu i zapisem po restarcie.
Porównanie Memory przed/po optymalizacji dla 6 modeli i 3 sample rate: różnica 0.
Wszystkie źródła są lokalne do JerzyMultiFX; źródłowe repozytoria bez zmian.
Natywny ARM64, jego JACK dummy oraz gotowa paczka są jeszcze w przygotowaniu;
nie traktować tego wpisu jako potwierdzenia publikacji. Fizyczny Pi/HeadRush/radio
i odsłuch nie były sprawdzone. Nie zmieniono zainstalowanej wersji Windows VST.

## Preview 4 — końcowy instalator RPi opublikowany i pobrany

Źródłowy commit **19c05423a0b1b3be744131f1becaf2cdbf689638**.
Natywny ARM64: **SUCCESS**
https://github.com/jurekbak71-dev/JerzyMultiFX/actions/runs/40725074577 .
Release:
https://github.com/jurekbak71-dev/JerzyMultiFX/releases/tag/v0.1.0-preview.4.ci.40725074577.1 .
Wydanie RPi; Windows celowo pominięty. Wcześniejszy run 40724632983 także przeszedł,
ale końcowy powyższy obejmuje dodatkową poprawkę zapisu po LOAD bez dalszej edycji.
Wcześniejszych wydań ani plików nie kasowano.

- Natywny ARM64: CTest 3/3 (DSP/RT, sygnał i latencja, kontrolki RPi/autosave),
  Python 28/28, Node 9/9, HTTP/core/IR/preset/BPM/mono/CC, restart po LOAD i po MIDI,
  rzeczywisty JACK dummy z sygnałem, MIDI i zmianą bufora — PASS.
- Paczkowanie, architektura i prywatność źródeł — PASS w CI.
- Po publikacji pobrano gotowy ZIP instalatora, TAR.GZ i manifesty. Zewnętrzne i
  wewnętrzne SHA256, oba ELF64 AArch64, SHA źródeł, nowe kontrolki/DSP/odzyskiwanie,
  składnia osadzonego instalatora i identyczność bajtowa jego payloadu z TAR.GZ — PASS.
- Mikrobenchmark CI ARM64, 48 kHz/128, CPU sekund na sekundę audio: Tape Warm
  0.020161, Cloud 0.008835, Shimmer 0.009457, Memory 0.008598. To pomiar runnera ARM64,
  nie fizycznego Raspberry Pi; nie jest gwarancją obciążenia całego toru użytkownika.

Instalator: `JerzyMultiFX-INSTALATOR.zip` w powyższym Release. Rozpakować na Windows,
włączyć HeadRush i Raspberry Pi, uruchomić `ZAINSTALUJ-NA-RASPBERRY.cmd`.
Instalator zachowuje sieć/JACK/presety, dodaje autostart/recovery oraz rollback.
Nie instalowano zdalnie na fizycznym bassambient. Radio Bluetooth/USB kontroler
właściciela, odsłuch i rzeczywista latencja HeadRush nadal wymagają odbioru na
sprzęcie. Nie potwierdzono identyczności Cloud z urządzeniami Strymon.

## Master prompt 29.10.2026 — aktualny zakres i MIDI

Właściciel dołączył rozszerzony master prompt obejmujący pełny produkt,
instalatory EXE/DEB, diagnostykę i AUTO LATENCY TUNE. Prace i kryteria otwarte
są w `TASKS.md`. Istniejące preview 3/4 pozostają rzeczywistymi binariami,
ale nie spełniają wszystkich nowych kryteriów, w szczególności fizycznego odbioru,
graficznych EXE i pakietu DEB.

Po zgłoszeniu niedziałających bypassów MIDI dodano do STATE licznik komunikatów
i ostatni odebrany status/CC/wartość z rzeczywistego callbacku JACK, także w
panelu MIDI. Dzięki temu panel pokaże, czy CC 0/127 dotarł do rdzenia i czy ma
przypisany parametr. Logika CC nadal stosuje 0=ON i 127=BYPASS; sama nazwa
trybu Toggle w kontrolerze nie jest dowodem, że rdzeń odbiera komunikaty.
Fizyczny Ampero/Pi nadal nie został sprawdzony; weryfikacja kompilacji i testów
tej zmiany zostanie podana po ich ukończeniu. Dotychczasowy instalator preview 4
nie zawiera nowego wskaźnika MIDI.


### Preview 5 — diagnostyka MIDI, opublikowana

Właściciel potwierdził, że Ampero już działa. To potwierdzenie dotyczy jego
bieżącej instalacji, a nie testu fizycznego nowej paczki. Wskaźnik ostatniego
CC w panelu jest dodatkowym narzędziem na wypadek kolejnej awarii.
Źródło **cdbceb38c44fada9d107c4dfed09a879d900e4b6**; natywny ARM64 **SUCCESS**:
https://github.com/jurekbak71-dev/JerzyMultiFX/actions/runs/40834602630 .
Release: https://github.com/jurekbak71-dev/JerzyMultiFX/releases/tag/v0.1.0-preview.5.ci.40834602630.1 .
Natywne CTest 3/3, Node 9/9, Python 28/28, HTTP/core z ostatnim CC, JACK dummy/audio
oraz pakowanie i kontrola prywatności — PASS. Gotowe paczki pobrano z Release:
SHA256 zewnętrzne/wewnętrzne, oba ELF64 AArch64, źródłowy SHA i identyczny payload
instalatora ZIP/TAR.GZ — PASS. Nie instalowano preview 5 na fizycznym Pi. Nowe
kryteria master prompt (EXE/DEB, AUTO LATENCY TUNE, pomiar fizycznej pętli) nadal
są otwarte w TASKS.md.


## Preview 6 — wspólne MIDI DELAY i globalne presety

Dla każdego banku DELAY A/B dodano trzy jawne cele MIDI Learn: TIME, FEEDBACK,
POZIOM EFEKTU / MIX. Zachowane adresy `delayA/B.time`, `feedback`, `mix` służą
jako stabilne cele mapowania; dopiero odebrany CC jest kierowany do aktualnego
silnika Digital, MX (także drugi slot, jeśli aktywny), Memory, Reel lub Oil.
Zmiana silnika nie kasuje jego zapamiętanych ustawień. `delayA/B.level` jest już
wspólnym wyjściowym gainem; MIX oznacza udział efektu, nie gain wyjściowy.

`program0`–`program127.jmfx` zapisują cały globalny preset i jego mapę MIDI.
Program Change 0–127 wczytuje ten plik, a pusty slot 0–7 wybiera fabryczny
preset z fabryczną mapą. Zmieniono również selekcję fabryczną z panelu, aby
przechodziła tą samą ścieżką. Poprzednie `.jmfx` i identyfikatory są zgodne.
Lokalny CTest 3/3, HTTP/core z rzeczywistym przejściem przez wszystkie pięć
silników obu banków, drugim slotem MX oraz dwiema różnymi mapami PC — PASS.
Natywna paczka ARM64 jest w przygotowaniu, nie zainstalowano jej na fizycznym Pi.

### Preview 6 — opublikowany instalator RPi

Źródłowy commit **d5ceca650c5feacb19ea5aadf1c2d844e8152874**.
Natywny run ARM64 **SUCCESS**:
https://github.com/jurekbak71-dev/JerzyMultiFX/actions/runs/40835486341 .
Release:
https://github.com/jurekbak71-dev/JerzyMultiFX/releases/tag/v0.1.0-preview.6.ci.40835486341.1 .
CTest 3/3, Python 28/28, Node 9/9, HTTP/core z mapowaniem MIDI na wszystkie
silniki i sloty PC, JACK dummy/audio/MIDI, paczkowanie/allowlista — PASS.
Po pobraniu ZIP instalatora, TAR.GZ i manifestów sprawdzono SHA256, oba binaria
ELF64 AArch64, SHA źródeł, prywatność źródeł i identyczny payload instalatora — PASS.
Fizyczny Pi/HeadRush i Hotone nie zostały testowane z preview 6; wcześniejsze
potwierdzenie działania Ampero dotyczy instalacji właściciela sprzed preview 6.
Windows VST nie był w tym przebiegu budowany.


## Preview 7 — automatyczny raport sprzętu RPi

Aktualizator zapisuje `diagnostyka-przed.json` i `diagnostyka-po.json` w prywatnym
katalogu instalacji. Nowy, ograniczony czasowo kolektor czyta wyłącznie jawnie
wymienione dane ALSA/USB/MIDI, model/kernel/CPU/RAM oraz status usług i STATE
MultiFX. Pomija tokeny, zmienne środowiska, logi, unikatowy Serial/Revision CPU.
Wynik podaje wyliczone opóźnienie algorytmu i pojedynczego okresu; fizyczny
round-trip ma wartość null. Szczegóły: AUDIO_LATENCY_ANALYSIS.md.

Wykonano Python 30/30, py_compile oraz diff --check PASS. Nie uruchomiono
kolektora na fizycznym Pi; jego dostępne urządzenia i realne opóźnienie nie są
potwierdzone. Natywny ARM64 i paczka są w przygotowaniu.

### Preview 7 — opublikowane i zweryfikowane pliki

Źródło `e207a59703d984702258a567da6cff0512f28ae4`. Natywny ARM64 SUCCESS:
https://github.com/jurekbak71-dev/JerzyMultiFX/actions/runs/40837981375 .
Release: https://github.com/jurekbak71-dev/JerzyMultiFX/releases/tag/v0.1.0-preview.7.ci.40837981375.1 .
CTest 3/3, Python 30/30, Node 9/9, HTTP/core, JACK dummy/audio/MIDI, pakowanie — PASS.
Pobrany ZIP i TAR.GZ: zewnętrzne/wewnętrzne SHA256, ELF64 AArch64, źródłowy SHA,
identyczny payload, obecność i wywołanie kolektora przed/po — PASS.
Pierwsze lokalne sprawdzenie manifestu użyło błędnie przepisanego SHA jako oczekiwanego;
po podaniu dokładnego SHA z BUILD-INFO cała kontrola przeszła. Nie był to błąd paczki.
Nie instalowano preview 7 na fizycznym Pi. Realny round-trip i AUTO LATENCY TUNE
są nadal otwartymi zadaniami.

## Preview 8 — pakiet Debian ARM64

Dodano równoległy `.deb` budowany wyłącznie ze sprawdzonego katalogu paczki
ARM64. Ma metadane `Architecture: arm64`, zawiera ten sam binarny payload i
`jerzy-multifx-setup`. `dpkg -i` nie uruchamia usług, nie ma skryptów maintainer
i nie zmienia JACK. Osobny setup sprawdza SHA256 i wywołuje istniejący
instalator z kopią zapasową; uruchomienie audio wymaga jawnego `--enable --device`.

Lokalny test struktury `.deb`, zgodności plików z katalogiem źródłowym oraz
braku automatycznej aktywacji — PASS. Pełny natywny build ARM64, weryfikacja
gotowego pakietu i publikacja tej rewizji czekają na CI. Fizyczna instalacja,
odsłuch, round-trip i AUTO LATENCY TUNE nie zostały wykonane.

Analiza patchOS/MODEP: `docs/PATCHOS_ANALYSIS.md`. W dedykowanym wariancie JACK
dodano 5 s między próbami startu przy odłączonym USB. Nie zmienia to istniejącego
systemowego JACK na `bassambient`; brak pomiaru, który uzasadniałby kopiowanie
ustawień ES-8/ES-9 lub Pisound na HeadRush.

### Kontrola opublikowanego preview 8 i poprawka preview 9

CI preview 8 przeszło na ARM64 i pobrany `.deb` miał ten sam payload co TAR.GZ.
Dodatkowa kontrola listy TAR wewnątrz DEB wykazała jednak tryb `700` dla wpisu
głównego `./`, odziedziczony po prywatnym katalogu tymczasowym. Preview 8 DEB
nie należy instalować. Preview 9 ustawia `755` i sprawdza ten warunek w teście
jednostkowym oraz weryfikatorze gotowej paczki. Natywne CI i ponowne pobranie
preview 9 są wymagane przed oznaczeniem tej poprawki jako wydanej.

Natywne CI preview 9: ARM64, testy i publikacja SUCCESS:
https://github.com/jurekbak71-dev/JerzyMultiFX/actions/runs/40855140222 .
Pobrany `.deb`, TAR.GZ i instalator ZIP mają poprawne zewnętrzne SHA256, zgodny
SHA źródła `6f482a59ec85fc404a49ec5e3995a10e945b783f`; `.deb` ma `755`
na `./` i byte-identyczny payload z TAR.GZ. W tym samym runie Windows przeszedł
kompilację, test edytora, walidator i pakowanie, lecz **publikacja VST3 nie powiodła
się**: Python na runnerze dekodował JSON z polskim opisem Release jako cp1252.
Preview 9 RPi jest gotowe programowo, ale cały run ma status failure; nie
deklarujemy Windows preview 9 jako wydanego.

## Preview 10 — poprawka publikatora Windows

GitHub CLI jest teraz czytany jawnie jako UTF-8, z testem tej ścieżki.
Natywny CI, publikacja i ponowne pobranie obu platform nadal oczekują.

RPi ARM64 w preview 10 przeszło natywny build i testy. Pobrano DEB, TAR.GZ
oraz instalator ZIP; zewnętrzne/wewnętrzne SHA256, źródło
`103a37ed408a5d753b5695e16a41571d762f43f8`, ELF AArch64,
identyczność payloadu DEB/TAR i bezpieczne uprawnienia — PASS.
Windows w chwili tego wpisu nadal trwa; właściciel wskazał RPi jako priorytet.
Fizyczna instalacja i odsłuch na HeadRush pozostają niewykonane.

## Preview 11 — graf JACK w diagnostyce RPi

Raport przed/po aktualizacji zapisuje `jack_lsp -c` oraz `jack_lsp -l`.
To pozwala skontrolować połączenia portów HeadRush i zgłaszaną przez JACK
latencję portów; nie zastępuje fizycznego pomiaru round-trip. Istniejący JACK
na bassambient nie jest przestawiany. Natywna walidacja i wydanie RPi czekają.

Wydanie RPi preview 11: natywny ARM64 SUCCESS:
https://github.com/jurekbak71-dev/JerzyMultiFX/actions/runs/40857009450 .
CTest 3/3, Python 33/33, Node 9/9, HTTP/core, JACK dummy/audio/MIDI (0 xrun
w przebiegu testowym), pakowanie i publikacja — PASS. Pobrane z Release DEB,
TAR.GZ i instalator ZIP: zewnętrzne i wewnętrzne SHA256, ELF64 AArch64,
źródło `586b4ac9d044753072e2b12611397914227b03d3`, payload DEB/TAR,
uprawnienia oraz nowy kolektor w instalatorze — PASS.
Release: https://github.com/jurekbak71-dev/JerzyMultiFX/releases/tag/v0.1.0-preview.11.ci.40857009450.1 .
Job Windows został świadomie pominięty ze względu na priorytet RPi; wcześniejszy
preview 10 Windows zakończył się sukcesem CI, ale nie był częścią tego wydania.
Brak fizycznej instalacji na bassambient, pomiaru round-trip, odsłuchu A/B i
potwierdzenia reakcji USB/BLE na tym preview.

## Referencje do dalszych prac RPi

Na prośbę właściciela przejrzano `react-guitar`, `brummer10/guitarix` oraz
`Quinny/GuitarEffects`. Konkretne commity, licencje, przydatne rozwiązania i
odrzucone wzorce realtime opisuje `docs/REFERENCJE_GITARA_RPI.md`.
Nie dodano ich kodu do paczki ani nie deklarowano testu na HeadRush.

## Wymaganie odłożone — MAKEUP kompresorów

Właściciel polecił zapamiętać, lecz **jeszcze nie wykonywać**: we wszystkich
kompresorach MAKEUP musi móc ściszać i wzmacniać; 0 dB ma być na środku.
Pełna lista sześciu obecnych parametrów i uwaga o migracji są w `TASKS.md`.
Nie zmieniono DSP, panelu, identyfikatorów parametrów, wersji ani paczek;
nie uruchamiano nowej kompilacji.
