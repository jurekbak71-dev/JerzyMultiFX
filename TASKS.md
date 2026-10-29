# JerzyMultiFX — prace po master prompt właściciela (29.10.2026)

Status oparty na istniejącym kodzie i testach; nie uznaje testu na runnerze za odbiór sprzętowy.

Referencje wskazane 29.10.2026: [react-guitar, Guitarix, GuitarEffects](docs/REFERENCJE_GITARA_RPI.md). Decyzje dla RPi zapisano tam trwale; kodu tych projektów nie kopiowano. Priorytet właściciela: RPi/HeadRush przed VST.

Nowe wymaganie właściciela (tylko zapis, **jeszcze nie implementować**): we wszystkich kompresorach regulator **MAKEUP** ma obejmować wartości ujemne i dodatnie w dB, z **0 dB dokładnie pośrodku**. Obrót w lewo ścisza, obrót w prawo podgłaśnia. Dotyczy `comp.makeup`, `td.optoMakeup` oraz kompresorów wejścia/wyjścia Oil w bankach DELAY A i B: `delayA.oil.icmakeup`, `delayA.oil.ocmakeup`, `delayB.oil.icmakeup`, `delayB.oil.ocmakeup`. Obecnie TD ma w DSP zakres −12…+12 dB i punkt neutralny przy wartości 0,5, ale panel nie opisuje go w dB; pozostałe wskazane parametry mają zakres 0…+24 dB. Przy przyszłej zmianie zachować stabilne ID/klucze oraz migrację istniejących presetów i map MIDI; dokładny zakres nowej regulacji ustalić podczas implementacji, bo właściciel nie wskazał liczby dB.

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
- [ ] Po odblokowaniu przez właściciela: bipolarne MAKEUP we wszystkich wymienionych kompresorach, 0 dB w środku, czytelne wartości dB w RPi/VST, zgodność presetów i MIDI. Teraz pozostawić kod bez zmian.

Nie oznaczać żadnego z otwartych punktów jako ukończony bez odpowiednich wyników.
