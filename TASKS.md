# JerzyMultiFX — prace po master prompt właściciela (29.10.2026)

Status oparty na istniejącym kodzie i testach; nie uznaje testu na runnerze za odbiór sprzętowy.

Referencje wskazane 29.10.2026: [react-guitar, Guitarix, GuitarEffects](docs/REFERENCJE_GITARA_RPI.md). Decyzje dla RPi zapisano tam trwale; kodu tych projektów nie kopiowano. Priorytet właściciela: RPi/HeadRush przed VST.

MAKEUP: kompresor główny i cztery kompresory Oil Drum mają już −24…+24 dB, Tape Drive −12…+12 dB z etykietą w panelu, Attack/Decay −12…+12 dB; neutralne 0 dB jest pośrodku. Stare projekty MultiFX zachowują poziom dzięki migracji.

- [x] Wspólny DSP, Windows VST3 x64 i natywne ARM64 oraz paczki preview 3/4 w prywatnym GitHub Release.
- [x] RPi instalator ZIP z dwuklikiem CMD, zachowaniem presetów, backupem i rollbackiem; panel bez wpisywania klucza.
- [x] USB/BT MIDI relay i MIDI Learn; dodany licznik oraz ostatni rzeczywiście odebrany komunikat CC w panelu, aby odróżnić brak MIDI od błędnego przypisania.
- [x] Właściciel potwierdził 29.10.2026: Ampero już działa i przełącza efekty na jego instalacji. Nie przypisywać tego potwierdzenia nowemu preview 5 bez instalacji.
- [x] Automatyczny kolektor przed/po instalacji: ALSA, USB, MIDI, usługi, model/CPU/RAM, stan MultiFX i rozdzielenie latencji wyliczonej od niezmierzonego round-trip. Nie uruchomiono go jeszcze na fizycznym Pi.
- [ ] Pomiar round-trip na fizycznej pętli audio; oddzielić od latencji obliczonej z bufora i algorytmów.
- [ ] AUTO LATENCY TUNE na wspieranym urządzeniu, z wyciszeniem podczas zmian i zachowaniem bezpiecznej konfiguracji.
- [ ] Uniwersalny manager USB Audio; obecny instalator HeadRush jest celowo dopasowany do istniejącego bassambient.
- [x] Pakiet DEB ARM64: natywne CI i pobrana paczka preview 10 potwierdziły architekturę, SHA256, zawartość zgodną z TAR.GZ oraz bezpieczne uprawnienia. Instalacji na fizycznym Pi jeszcze nie wykonano.
- [ ] Graficzny EXE Windows VST3 i graficzny EXE instalatora RPi. ZIP + CMD i TAR.GZ nadal dostępne; właściciel wskazał RPi jako priorytet.
- [ ] Fizyczny odsłuch i pomiary CPU/xrun przy reprezentatywnym łańcuchu, w tym Cloud i Tape Warm.
- [x] Bipolarne MAKEUP w kompresorach MultiFX z migracją starych presetów.
- [x] Ujednolicić RPi/VST DELAY Memory/Reel/Oil Drum z MX: ON/OFF jako przełączniki, BANK MIX u góry jako suwak w widoku podstawowym i zaawansowanym, opcje jako listy lub pola liczbowe.
- [x] Mapa konkretnych kontrolerów Hotone Ampero Control + EXP i WORLDE EASYCONTROL.9 w ustawieniach RPi; czyszczenie wszystkich przypisań CC/ekspresji i odłączenie własnych slotów PC bez usuwania brzmień ani parowania.
- [ ] Jeśli potrzebne: rozdzielenie CC po porcie/kanałach urządzeń i osobny zakres/odwrócenie pedału ekspresji w każdym presecie.

Nie oznaczać żadnego z otwartych punktów jako ukończony bez odpowiednich wyników.
