# JERZY MULTIFX — GŁÓWNY PROMPT AUTONOMICZNEGO WDROŻENIA

## ROLA I CEL

Jesteś głównym inżynierem DSP, VST3, Linux Audio, Raspberry Pi, GUI, MIDI, CI/CD i integracji systemowej. Masz wykonać **rzeczywisty projekt produkcyjny** `jurekbak71-dev/JerzyMultiFX`: jeden multiefekt z pięcioma przestawialnymi klockami, **jednym wspólnym silnikiem DSP**, dwoma pierwszorzędnymi produktami: **VST3 Windows x64 / FL Studio** oraz **samodzielny MULTIFX Raspberry Pi**. LV2 będzie późniejszym celem: nie musi być ukończony w pierwszym wydaniu, ale architektura ma go nie blokować.

Działaj maksymalnie samodzielnie: analizuj repozytoria, podejmuj uzasadnione decyzje inżynierskie, wdrażaj, testuj, poprawiaj błędy, wykonuj kolejne etapy i dokumentuj postępy. **Nie zadawaj pytań o szczegóły projektowe możliwe do ustalenia na podstawie kodu, dokumentacji, testów albo bezpiecznych założeń.** Nie udawaj jednak posiadania nieistniejącego sprzętu, niedostępnych uprawnień czy przeprowadzonych testów. Kiedy wymagana jest zewnętrzna autoryzacja, zgoda na dostęp, prawdziwe urządzenie lub nieznany model interfejsu audio, wyodrębnij to jako test sprzętowy i kontynuuj niezależne zadania. Nigdy nie publikuj niezweryfikowanego buildu jako finalnego.

Nie wolno niszczyć, resetować ani zmieniać istniejących repozytoriów źródłowych; **tylko nowe repo `JerzyMultiFX` jest celem zapisu**. Nie używaj `--force` na gałęziach źródłowych, nie nadpisuj repozytoriów, nie usuwaj istniejących danych użytkownika, nie umieszczaj tokenów w kodzie i logach.

## REPOZYTORIA — INWENTARYZACJA I ZAPOŻYCZENIA KODU

Zidentyfikuj oraz uzyskaj odczyt z repozytoriów konta `jurekbak71-dev`:

1. `JerzyMXAnalogDelayVST3`
2. `JerzyTapeDriveVST3`
3. `JerzySmallCloneChorus`
4. `JerzyMemoryDelay`
5. `JerzyReelEchoVST3`
6. `JerzyAttackDecayVST3`
7. `JerzyAtomicClusterVST3`
8. `JerzyOilDrumEcho`
9. `JerzyAudioPlatform` — główny fundament aplikacji Raspberry Pi
10. `JerzyBigMuffCollectionVST3`
11. `JerzyRPiAudioLab` — dodatkowe źródło doświadczeń z RPi, nie zamiennik `JerzyAudioPlatform`.

Sprawdź rzeczywiste nazwy repozytoriów, najnowsze stabilne commity, struktury DSP, zależności, licencje, ograniczenia i bieżące bugi. Wygeneruj `docs/SOURCE_AUDIT.md` z mapowaniem fragmentów kodu -> nowych komponentów, commit SHA, licencjami oraz listą wykorzystanych rozwiązań. Nie zakładaj, że nazwa repo oznacza gotowy działający kod. Przejrzyj obowiązki licencyjne (w tym GPL-3.0, AGPL-3.0 widoczne w części repozytoriów, JUCE jeśli użyte, zależności VST3 i elementy o niejasnej licencji); wybierz darmowe i prawnie zgodne zależności, utrzymaj odpowiednie noty i udostępnianie kodu wymagane przez licencje. Jeśli jakaś licencja blokuje dany sposób dystrybucji, zastąp komponent własnym algorytmem lub prawidłową alternatywą i opisz to; nie kopiuj nieuprawnionego kodu.

Otwórz `references/` (jeśli użytkownik dołączy ZIP ze schematami) i porównaj układy TS808, DS-1/DS-2 oraz kompresory z kodem. Schemat **BOSS DM-2** służy wyłącznie jako ewentualne źródło pomysłów do DELAY, **NIE do DRIVE**. Referencje schematów są inspiracją do modelowania nieliniowości i dynamiki, a nie gwarancją pełnej identyczności brzmienia.

## ARCHITEKTURA

- C++17/20 i CMake, niezależna od API hosta biblioteka `multifx_dsp` bez zależności od GUI, systemu okien i audio backendów.
- Adapter VST3 Windows x64 z pełnym raportowaniem parametrów hostowi; osobny frontend RPi pod Linux ARM (aarch64) z dostępem do audio, MIDI, Bluetooth, HTTP/WebSocket i LCD 1602A; zostaw czytelny adapter/kontrakt dla późniejszego LV2.
- Nie uruchamiaj istniejących wtyczek VST wewnątrz RPi. Zrefaktoryzuj algorytmy DSP do kodu natywnego, bez zewnętrznych mostów między formatami.
- Jeden identyczny model parametrów ze stałymi unikatowymi ID, zestaw stanów, presety, migracje wersji, serializacja kompletnego łańcucha i referencji IR. Udostępnij hostowi wszystkie parametry wymagające automatyzacji, wraz z nazwami, jednostkami i sensownymi zakresami.
- Audio realtime safe: **zero alokacji, blokujących locków, I/O dyskowego lub sieciowego na audio thread**. Denormal handling, interpolacja i smoothing parametrów, bounds checks, brak clicków przy bypassie/zmianie routingu/engine, oversampling gdzie potrzebny, ochrona przed NaN/Inf/feedback runaway, kontrola CPU i RAM na RPi.
- Wspieraj mono i stereo, 44.1/48/96 kHz i zmienny rozmiar bloku; bezpieczne dostosowanie stereo/mono, sample rate, latencji i delay compensation. Gdy parametr wymaga przealokowania pamięci, rób to poza audio thread lub bezpiecznie przełącz gotowy stan.
- Pięć głównych klocków można przeciągać w dowolnej kolejności. Każdy ma własny bypass, stan i presety, a zmiany kolejności muszą być bezpieczne w czasie odtwarzania. Nie myl kolejności **klocków** z wewnętrznym porządkiem sekcji.

## KLOCEK 1 — ANALOG PROCESSOR

Jeden klocek z czterema sekcjami w domyślnym porządku `CPD -> TD -> DRIVE -> COMP`:

### A. CPD, sekcja wejściowa
Nazwa CPD pochodzi od Compressor / Preamp / Drive, ale użytkownik jawnie wyspecyfikował sekcję PREAMP, a osobne końcowe COMP i DRIVE występują niżej. **Nie dubluj niepotrzebnie obwodów kompresora ani drive w sekcji wejściowej.** Utrzymaj nazwę CPD w GUI, obsłuż jego tor wejściowy, zachowaj miejsce na dalszą ewolucję. PREAMP posiada wybór `CLEAN`, `SOLID STATE`, `TUBE`, oraz `GAIN`, `OUT VOL`. Czysty, tranzystorowy i lampowy charakter mają być faktycznie odróżnialne harmonicznie/dynamicznie. Dla całego klocka ustaw bezpieczny headroom i gain staging.

### B. TD — TAPE DRIVE
Bazuje na `JerzyTapeDriveVST3`. Zawiera:
1) GAIN/SATURATION o dwóch zakresach: pierwszy od clean/warm do saturacji, drugi od mocnego przesterowania po skrajną destrukcję;
2) sekcję MECHANICS z emulacją silnika, paska napędowego, mechaniki taśmy, zużycia taśmy, starzenia elektroniki, artefaktów, wow, flutter, modulacji chorus/flanger, ale bez przypadkowych niekontrolowanych trzasków destabilizujących system;
3) końcowy ANALOG FILTER: HPF FREQ/Q oraz LPF FREQ/Q;
4) GAIN, OUT VOL i DRY/WET MIX. Wykorzystaj istniejące rozwiązania i ich nazwy/parametry tam, gdzie sensowne. Umożliwiaj muzykowi odzyskanie czystego sygnału bez zwiększania głośności po załączeniu efektu.

### C. DRIVE
Jeden moduł wybierający spośród trzech brzmień:
- FUZZ — wybierz jeden muzykalny, najlepszy silnik z `JerzyBigMuffCollectionVST3`, uzasadnij wybór testami i reakcją na dynamikę;
- OVERDRIVE — model TS808 Tube Screamer inspirowany przekazanymi schematami: input buffer, nieliniowe sprzężenie op-amp z diodami 1N4148, tone, level, output buffer, typowy mid-hump i soft clipping;
- DISTORTION — inspiracja analogowymi BOSS DS-1 oraz DS-2 (w tym cechy Turbo z DS-2), przełączane rozwiązania charakterystyki w ramach trybu, zachowanie obwodowego clippingu, gain/tone/level. DM-2 NIE jest distortion.
Dopasuj czytelne, muzyczne regulatory do każdego trybu; unikaj skoków głośności i aliasingu.

### D. COMP — KOŃCOWY COMPRESSOR/LIMITER
**Jeden autorski algorytm**, nie kolekcja wielu typów. Wykorzystaj najlepsze cechy referencji: Hollis Flatline LED/LDR, Colorsound SupaSustain, Ross/Dyna Comp OTA, Clean Squeeze Optical, Craig Anderton Optical oraz inne pokazane schematy. Priorytety: naturalne transjenty, dobry sustain, muzykalna kompresja, brak pompowania i niepożądanych zniekształceń. Regulatory `ATTACK`, `DECAY/RELEASE`, `COMPRESSION`, `KNEE`, `INPUT LEVEL`, `MAKEUP GAIN`, `LIMITER ON/OFF`. Mierniki rzeczywistego `IN`, `OUT`, `GR` (dB) aktualizowane bez obciążania audio thread. Limiter z kontrolowaną pracą i określonym ceiling; nie deklaruj true-peak bez prawdziwej implementacji i testów.

## KLOCEK 2 — TRZY NIEZALEŻNE SILNIKI
Jedna obudowa/GUI, trzy niezależne silniki DSP z własnym bypass i wszystkimi parametrami oryginalnych wtyczek:
- `JerzySmallCloneChorus`
- `JerzyAttackDecayVST3`
- `JerzyAtomicClusterVST3`

Nie redukuj funkcji do demonstracyjnego LFO, ADSR czy dwóch gałek. Przenieś całą semantykę parametrów, presetów i charakteru brzmienia. Zweryfikuj oddzielną pracę każdego silnika oraz ich wspólną pracę. Nie narzucaj dodatkowych zaawansowanych trybów routingu bez potrzeby; priorytet to zgodność funkcjonalna.

## KLOCEK 3 — MD MULTIDELAY
Dwa niezależne procesory `DELAY A` i `DELAY B`, każdy z **pełną listą wyboru algorytmów** z: wszystkich unikatowych silników `JerzyMXAnalogDelayVST3`, `JerzyMemoryDelay`, `JerzyReelEchoVST3`, `JerzyOilDrumEcho` oraz nowego, czystego `DIGITAL DELAY`.

Analizuj duplikaty: np. Oil Echo może zastąpić pokrywający się wariant w MX; usuwaj duble tylko po porównaniu funkcjonalnym. Zapewnij 3 routingi:
- `SERIAL`: A -> B;
- `PARALLEL`: oba z tego samego wejścia i sumowane niezależnymi poziomami;
- `MIX`: jawny A/B crossfade/mixer i regulacja miksu obu przetworzonych ścieżek (odróżnij w GUI od routingu parallel).

Wzorcem GUI i ergonomii są parametry oraz układ `JerzyMXAnalogDelayVST3`: mechanika urządzeń, wielogłowicowość gdzie występuje, degradacja, zużyta elektronika, emulacja wieku/stanu `NEW`, `SERVICED`, `USED`, `WORN`, odpowiednie artefakty. Każdy A/B ma niezależny typ urządzenia, `time`, sync do host BPM/transportu, feedback, tone/filter, dry/wet, output, stan mechaniczny i pozostałe parametry właściwe silnikowi. Dla digital delay — czysty powtarzalny timing, sensowne sterowanie (delay time, feedback, filtering, modulation jeśli wymagana, mix). Nie wywołuj błędów poprzednich projektów: trzasków multihead, feedbacku wchodzącego w samowzbudzenie niemal od zera, crash/freeze, zaniku audio. Zadbaj o pętle sprzężeń i stabilny ogranicznik.

## KLOCEK 4 — REVERB
Pięć wybieralnych silników `PLATE`, `ROOM`, `CATHEDRAL`, `CLOUD`, `SHIMMER` z klasycznymi regulatorami `DECAY`, `PRE-DELAY`, `SIZE`, `DAMPING`, `LOW CUT`, `HIGH CUT`, `DIFFUSION`, `MODULATION`, `MIX`, `OUTPUT`. Dopasuj specjalistyczne regulatory CLOUD i SHIMMER (np. pitch interval/feedback) bez ukrywania kluczowych funkcji. Reverb ma mieć dobre ogony, stabilną polifoniczną przestrzeń, brak niekontrolowanych skoków, anty-denormal i bezpieczne maksima feedbacku. Nie utożsamiaj reverb z IR loaderem.

## KLOCEK 5 — IR LOADER
Samodzielny konwolucyjny `IR LOADER`: wybór plików WAV z PC, nazwa załadowanego IR, bypass, dry/wet, out gain, sensowne mono/stereo, konwersja sample-rate, kontrola długości oraz bezpieczna podmiana IR bez dropów i blokowania audio. Pamiętaj stan IR oraz rozwiązanie przenoszenia presetów i projektów FL: osadzenie w projekcie lub kopiowanie do kontrolowanej biblioteki z komunikatem o brakującym pliku. W RPi zapewnij import IR z PC przez lokalne GUI Wi-Fi/upload lub USB; przez BLE wystarczy wybór **już znajdujących się** IR, bez obietnicy przesyłania dużych WAV przez Bluetooth.

## GUI VST3 — WINDOWS I FL STUDIO

- **Jeden plugin VST3 64-bit** (nie pięć osobnych). Nazwa `Jerzy MultiFX`, stabilne plugin ID i param IDs.
- Nowoczesne, czytelne GUI ze schematem pięciu klocków; drag-and-drop kolejności; wewnętrzne zakładki i logiczny layout, regulatory i wskaźniki; nie projektuj mikroskopijnych opisów; skalowanie interfejsu i prawidłowe DPI.
- Pełny host parameter exposure: MIDI learn CC dla każdego parametru (w granicach VST3 API), trwałe mapowanie MIDI, automatyzacja, host tempo/sync, bezpieczne rozpoznawanie MIDI controller vs host automation, presety, import/export. W FL Studio testuj poprawne zachowanie przy starcie, zamykaniu, ponownym otwarciu FLP, skanowaniu VST3, renderingu offline WAV, różnych sample rates, a jeśli FL niedostępne w CI, prowadź jasno oznaczoną checklistę testu manualnego.
- **ŻADNEGO automatycznie uruchamianego sekwencera** ani własnych nut po otrzymaniu transportu FL Studio. To jest multiefekt przetwarzający audio; projekt FL musi wiernie odtwarzać zapisane parametry podczas eksportu WAV.
- **Wszystkie wartości DSP i stan łańcucha muszą się odtwarzać** po otwarciu sesji i renderingu offline. Unikaj znanego z poprzednich projektów błędu GUI „czarne okno” w FL Studio. Zaimplementuj tryb zgodności renderera i smoke-test otwarcia GUI.

## RASPBERRY PI — JERZY AUDIO PLATFORM, WIFI I BLUETOOTH

`JerzyAudioPlatform` jest podstawą aplikacji i obsługi hardware. Przejrzyj go, ale **usuń stare efekty z finalnej listy**, a nowy łańcuch ma zawierać wyłącznie pięć opisanych klocków. `JerzyRPiAudioLab` może dostarczyć dodatkowy kod interfejsów/testów. Nie usuwaj z systemu użytkownika żadnych wcześniej zainstalowanych danych bez tworzenia kopii i jawnej procedury migracji.

- Natychmiast po starcie po skonfigurowaniu urządzenia uruchamiaj audio service (systemd), audio capture -> DSP -> audio output przy niskim opóźnieniu; autodetekcja ALSA / odpowiedniego backendu i kart (nie zakładaj modelu Raspberry Pi, interfejsu USB/I2S ani numeracji `hw:0,0`). Udostępnij kreator wyboru urządzenia, częstotliwości, bufora; pomiary xrun/CPU/RAM/latency.
- **USB-MIDI plug and play na wzór MODEP**: automatyczne wykrywanie podłączanych kontrolerów, mapowanie CC, MIDI Learn, obsługa footswitch, zapisywanie map; po restarcie zachowaj mapy. Zaprojektuj także standardowe sterowanie program change i expression pedal.
- **Wi-Fi GUI**: responsywne lokalne GUI WWW obsługujące całość MULTIFX, dostępne z telefonu i tabletu na istniejącej sieci Wi-Fi bez internetu; mechanizm odkrywania urządzenia mDNS i/lub adres IP; opcjonalnie tryb własnego punktu dostępowego AP w RPi z bezpiecznym hasłem, uruchamiany i wyłączany bez uszkodzenia sieci.
- **Bluetooth GUI, działające nawet gdy Wi-Fi jest WYŁĄCZONE**: oddzielny kanał sterujący BLE GATT z autoryzacją parowania, przesyłaniem stanów i zmian parametrów oraz powiadomień o zmianach. Nie opisuj zwykłej strony HTTP jako „GUI Bluetooth”: strona musi być osiągalna offline w kompatybilnym kliencie (np. wcześniej lokalnie zapakowany i zainstalowany interfejs, alternatywnie kompatybilna przeglądarka Web Bluetooth z zapamiętanymi zasobami i potwierdzonym cache). Dla iPhone/iPad sprawdź realną wykonalność z **Bluefy** lub zgodnym klientem iOS, ponieważ Safari nie ma natywnego Web Bluetooth. Dla Android sprawdź Web Bluetooth w Chrome lub aplikację kliencką. Jeśli offline GUI nie jest rzeczywiście weryfikowalne, wykonaj BLE API i klienta oraz zapisz uczciwy status testów; nie udawaj, że sama implementacja GATT oznacza działający GUI na iOS.
- **Bluetooth MIDI**: oddzielna od BLE GUI obsługa BLE MIDI, parowanie i odtwarzanie mapowania MIDI; wskaźnik podłączonych urządzeń, reconnect, obsługa konfliktów łączności.
- **LCD 1602A QAPASS, 16 znaków x 2 wiersze, 16-pin bez płytki I²C**: zgodny najprawdopodobniej z HD44780. Przygotuj sterownik wyświetlający preset, aktywny efekt, wybrane parametry i status MIDI/Wi-Fi/BLE/audio oraz sposób nawigacji, jeżeli wykryto/podłączono fizyczne przyciski lub enkodery. **Nie podłączaj 5V do 3.3V GPIO Raspberry Pi**. Zweryfikuj pinout i sposób pracy RW/RS/E/D4-D7, poziomy logiki, zasilanie, podświetlenie i ewentualny level shifter, zanim wygenerujesz definitywny schemat podłączenia. Wyraźnie rozróżnij działające testy logiczne od testu fizycznego LCD.
- Bezpieczeństwo: obsługa BT/Wi-Fi ma nie blokować wątku DSP; tylko lokalne interfejsy zarządzania, hasła i parowanie, bez otwartej administracji i publicznej ekspozycji sieciowej.
- Przygotuj `install.sh`/`uninstall.sh`/`diagnostics.sh`, backup oraz przywracanie poprzedniego stanu platformy; instalator ma działać na RPi aarch64 i sprawdzać system przed zmianami. Nie wykonuj formatowania kart SD ani resetów urządzenia bez bezpośredniej zgody właściciela.

## STRUKTURA REPOZYTORIUM (PRZYKŁAD, MOŻESZ ULEPSZYĆ)

```
JerzyMultiFX/
  CMakeLists.txt
  cmake/
  core/              # jednolity DSP, graf audio, modele parametrów/presetów
  effects/           # implementacje pięciu klocków i ich silników
  plugin-vst3/       # adapter hosta VST3, Windows GUI
  rpi/               # standalone app, ALSA, MIDI, BLE, Wi-Fi, LCD, usługi
  lv2/               # szkielet adaptera na później (bez blokowania VST/RPi)
  ui/                # wspólna logika prezentacji i control API
  tests/             # DSP, bezpieczeństwo RT, regresje GUI/CI
  scripts/           # build, packaging, pi installer, diagnostic, rollback
  references/        # schematy od użytkownika, jeśli dostarczone
  docs/              # instrukcje po polsku, przegląd licencji, hardware
  .github/workflows/ # Windows VST3, Linux ARM64, testy, paczki, release
  AGENTS.md          # trwałe instrukcje dla kolejnych sesji Codexa
  STATUS.md          # obiektywny stan, TODO, bloker i linki artefaktów
```

## TESTY — WYMAGANE PRZED WYDANIEM

1. Buduj DSP na Windows x64 i Linux ARM64; testuj 44.1/48/96 kHz, mono/stereo, bufory np. 32/64/128/256/1024 sampli, bypass, zmianę wszystkich bloków przy aktywnym audio, start/stop transportu, reset hosta, zmianę czasu delay i silników.
2. Zrób testy sygnałowe (impuls, sweep, gitara/bas, sygnał stereo), wykrywanie ciszy, NaN/Inf, niekontrolowanego clippingu, szczytów na zmianie presetów, stabilności pętli feedbacku.
3. Testuj zapisywanie/odczyt wszystkich presetów, map MIDI i plików IR, stan po restarcie hosta, porównanie offline render i real-time, identyczne param IDs.
4. Testy CPU/RAM przez dłuższy playback i stress: włączone wszystkie moduły, dwa delaye, shimmer i konwolucja; eliminuj wycieki i alokacje na audio thread.
5. Weryfikuj GUI na Windows render/screenshot i obsługę zmian; sprawdź crash podczas tworzenia/zamykania okna.
6. Wersja RPi: testy usług, virtual ALSA/dummy audio, symulacja USB-MIDI/BLE, LCD emulowany; generuj **osobną** listę testów wymagających fizycznego RPi i iPhone'a. Nie deklaruj „hardware tested”, jeżeli sprzętu nie było w runnerze.
7. Automatyczne testy licencji i spójności paczek: nie wydawaj pustego pliku ani tylko kodu źródłowego zamiast instalatora.

## CI/CD — KOMPLETNE PACZKI DO POBRANIA

- GitHub Actions Windows (`windows-2025` lub zgodny Windows x64 runner): CMake + Visual Studio Build Tools; buduj VST3 Release, zweryfikuj zawartość `.vst3`, spakuj `JerzyMultiFX-VST3-Windows-x64-vX.Y.Z.zip`, dołącz `INSTALACJA_VST3_PL.md` i checksums SHA256.
- GitHub Actions Linux ARM64 (`ubuntu-24.04-arm` lub kompatybilny runner): buduj samodzielny silnik i wersję RPi aarch64, zależności, testy; przygotuj `JerzyMultiFX-RPi-aarch64-vX.Y.Z.tar.gz` (opcjonalnie pakiet `.deb`) z działającym instalatorem, systemd service, narzędziem diagnostics, backup/uninstall i `INSTALACJA_RPI_PL.md`. Jeśli build musi powstać na docelowym RPi ze względu na GPIO/audio dependencies, daj w paczce deterministyczny build/install i wskaż ograniczenie; nie udawaj, że binarium x86_64 działa na ARM.
- Publikuj GitHub Actions **Artifacts** po pomyślnym CI. Dla wydania oznaczonego tagiem `vX.Y.Z` publikuj **GitHub Release** z binariami obu platform, opisem zmian, sumami SHA256, raportem testów i linkami do instrukcji. `contents: write` wyłącznie w dedykowanym jobie Release, inne joby domyślnie `contents: read`.
- Prowadź changelog, stabilną numerację semver, powtarzalny build. Nie oznaczaj `stable` bez testów funkcjonalnych i sprzętowych adekwatnych do deklarowanego statusu; użyj `pre-release`, gdy realnego sprzętu nie było.
- Każdy błąd kompilacji rozpoznaj z pełnego logu, napraw u źródła, uruchom ponownie, zrób regresję. Nie zatrzymuj pracy na pierwszym błędzie CI, jeśli masz możliwość naprawy.

## INSTRUKCJE DLA WŁAŚCICIELA — PO POLSKU

Przygotuj pełne dokumenty:
- `README_PL.md` — działanie multiefektu, 5 klocków, MIDI, bloki, routing, wymagania sprzętowe;
- `INSTALACJA_VST3_PL.md` — pobranie, rozpakowanie, ścieżka VST3 w Windows (`C:\Program Files\Common Files\VST3`), instalacja bezpieczna, skan FL Studio, instancja, sterowanie CC/automatyzacja, zapis projektu, offline WAV, troubleshooting czarnego GUI;
- `INSTALACJA_RPI_PL.md` — kompatybilny model RPi/system (po analizie), Raspberry Pi Imager, wykrywanie sprzętu audio, kabel audio, sieć Wi-Fi, SSH, przegranie paczki, wykonanie instalatora i jego flag, karta dźwiękowa ALSA, test audio, USB-MIDI, BLE MIDI, Bluetooth GUI i Wi-Fi GUI, LCD 1602A z bezpiecznym schematem GPIO, start po boot, diagnostyka, aktualizacja, reset/rollback, odinstalowanie;
- `WIFI_BLUETOOTH_PL.md` — dwa niezależne sposoby użycia GUI na iPhone/iPad i Android, zrozumiała instrukcja połączenia BLE bez Wi-Fi, rozróżnienie kontrolera BLE od przesyłania całej strony WWW;
- `HARDWARE_TESTS_PL.md` — weryfikacja sprzętu rzeczywistego, wymagana przy końcowych testach, bez zmyślania wyników;
- `SOURCE_AUDIT.md`, `LICENSES.md`, `STATUS.md`, `CHANGELOG.md`.

## TRYB AUTONOMICZNY, ROZPOCZĘCIE I KRYTERIA UKOŃCZENIA

1. Od razu przeczytaj wszystkie dostępne źródła, sprawdź możliwość kompilacji, sporządź `docs/SOURCE_AUDIT.md`, architekturę i testowalny backlog w `STATUS.md`, a potem **od razu implementuj** — nie kończ na planie.
2. Pracuj iteracyjnie i sekwencyjnie: DSP+testy -> VST3+GUI -> RPi audio/MIDI/Wi-Fi/BLE/LCD -> CI/release -> dokumentacja/QA. Nie zostawiaj atrap dźwięku ani przycisków GUI, które nic nie robią. Zapewnij sensowne testy po każdym etapie.
3. Zapisuj zmiany w małych commitach w nowym repozytorium/gałęzi projektu; stale aktualizuj `STATUS.md` z faktami i brakami. Gdy środowisko wymaga osobnego potwierdzenia GitHub push/PR, wykonaj wszystkie dozwolone kroki i jasno wskaż jedyną wymaganą autoryzację.
4. Masz moje upoważnienie do rozstrzygania zagadnień technicznych wewnątrz **nowego repo `JerzyMultiFX`** i do uruchamiania testów oraz GitHub Actions, ale NIE do rozszerzania dostępu do innych repo, manipulacji billingiem, kasowania prywatnych danych czy nieodwracalnych zmian w fizycznym RPi.
5. Nie kończ po samym `README`, szkielecie, GUI bez DSP, niedziałającym VST lub niekompletnym artefakcie. Jeżeli ograniczenie czasu zadania wymusi przerwę, zakończ stabilnym commitem, zaktualizuj `STATUS.md`, wypisz dokładną komendę / krótki prompt CONTINUE i nie nazywaj projektu zakończonym.
6. Sukces oznacza: jedną **rzeczywiście działającą** VST3 Windows x64, zweryfikowany build ARM64 oraz instalator RPi, działający pięcioklockowy łańcuch DSP, poprawne presety i mapowanie MIDI, Wi-Fi GUI oraz BLE GATT/MIDI, testy i instrukcje. Sprzętowy status RPi, LCD oraz iPhone'a może być oznaczony `AWAITING HARDWARE VALIDATION` do wykonania testów na fizycznych urządzeniach.
7. W końcowym raporcie podaj link do konkretnych **GitHub Release assets/Artifacts**, wersję, tag/commit SHA, wyniki testów Windows/ARM64, listę rzeczy sprawdzonych fizycznie i rzeczy nieweryfikowanych, a także bezpośrednią instrukcję instalacji VST3 oraz RPi. Jeśli link nie powstał, powiedz dlaczego; nie wymyślaj pliku ani linku.

**START. Realizuj projekt teraz, nie pytaj mnie o kolejne decyzje projektowe i nie zadowalaj się samym planem.**
