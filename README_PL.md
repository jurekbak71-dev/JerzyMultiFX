# Jerzy MultiFX 0.1.0-preview.2

Jeden multiefekt VST3 Windows x64 i samodzielna aplikacja Linux ARM64 oparta o wspólny silnik C++20. To **wydanie rozwojowe do testów**, nie produkt potwierdzony na fizycznym RPi ani w FL Studio. Aktualne fakty i braki: [STATUS.md](STATUS.md).

Pięć klocków można przestawiać: Analog Processor (CPD, Tape Drive, Drive, Compressor/Limiter), Chorus/AttackDecay/Atomic, MD Multidelay A/B, Reverb, IR Loader. Wewnętrzny porządek Analog Processor pozostaje CPD → TD → DRIVE → COMP. Tryby CPD: Clean/Solid State/Tube. DRIVE: Green Russian Fuzz, TS808-inspirowany Overdrive, DS-1/DS-2-inspirowany Distortion/Turbo. Są to modele behawioralne, bez obietnicy identyczności z analogowym oryginałem.

Chorus, AttackDecay, Atomic oraz MX, Memory, Reel i Oil Echo korzystają z natywnych źródeł oryginalnych projektów. MD ma jedną listę 13 silników na każdy bank i pokazuje parametry wybranego modelu. Pełne, zagnieżdżone mechanizmy MX pozostają w ustawieniach zaawansowanych. A i B mają niezależne ustawienia, a routing Serial, Parallel oraz Mix zmienia sposób ich połączenia. Nazwy i zakresy: `docs/PARAMETERS.json`.

Reverb wykorzystuje osiem linii FDN: Plate, Room, Cathedral, Cloud, Shimmer. IR jest oddzielnym blokiem konwolucji, z latencją 128 próbek przy aktywnej konwolucji i zerową po bypassie. Obsługiwane IR WAV mono/stereo PCM16/24/32 lub float32, maksymalnie 16384 próbek **po resamplingu**: około 371 ms/44,1 kHz, 341 ms/48 kHz, 171 ms/96 kHz. IR jest osadzany w stanie/presetiе `.jmfx`; projekt nie zależy od starej ścieżki pliku.

Interfejs Windows ma programowy renderer GDI, zakładki bloków i strony parametrów. Przeciągnij kafel, by zmienić kolejność; dwuklik przełącza bypass. Prawy klik regulatora uzbraja MIDI Learn. Host widzi 953 trwałe parametry oraz 128 ukrytych wejść CC. Parametry stanu mają stabilne klucze i identyfikatory; format z kontrolą CRC odrzuca uszkodzone dane.

RPi: JACK audio, USB-MIDI przez a2jmidid i natywny most BLE MIDI, panel HTTP z tokenem oraz biblioteką sterownika HD44780/LCD. Instalator HeadRush korzysta z istniejącej obsługi sprzętu/Bluetooth. Backend audio korzysta z architektury i kontraktów JerzyAudioPlatform, nie uruchamia VST w RPi i nie udostępnia jego starej listy efektów. Kod źródłowy JerzyAudioPlatform nie został zmieniony.

Najprostsza aktualizacja istniejącego bassambient: [dwuklik z Windows](INSTALACJA_RPI_PROSTO.txt).

Instrukcje: [Windows](INSTALACJA_VST3_PL.md), [RPi](INSTALACJA_RPI_PL.md), [Wi-Fi/BLE](WIFI_BLUETOOTH_PL.md), [testy sprzętowe](HARDWARE_TESTS_PL.md), [licencje](LICENSES.md).

## Budowanie i testy

W środowisku chmurowym: `source /workspace/.setup/activate.sh`, następnie `bash scripts/build.sh native`. Cross-build: `bash scripts/build.sh windows` lub `arm64`; wymagają wskazanych w `cmake/` toolchainów. Zwykły natywny build ARM na Debianie 13: `cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_FLAGS_RELEASE=-O2 && cmake --build build --parallel 3 && ctest --test-dir build --output-on-failure`.

Przed wydaniem uruchom testy CoreTests, `python3 -m unittest discover -s tests -p 'test_*.py'`, `python3 tests/integration.py`, `python3 tests/audio_integration.py`, walidator Steinberga na **bundle**, EditorSmoke, testy ARM/QEMU i walidację paczek. Instrukcje CI w `.github/workflows/build.yml`.
