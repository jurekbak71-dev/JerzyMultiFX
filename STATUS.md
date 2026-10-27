# Jerzy MultiFX — status

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

Repozytoria źródłowe nie są modyfikowane; pochodzenie kopii i SHA opisuje docs/source-manifest.json.

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
