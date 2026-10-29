# JerzyMultiFX — prace po master prompt właściciela (29.10.2026)

Status oparty na istniejącym kodzie i testach; nie uznaje testu na runnerze za odbiór sprzętowy.

- [x] Wspólny DSP, Windows VST3 x64 i natywne ARM64 oraz paczki preview 3/4 w prywatnym GitHub Release.
- [x] RPi instalator ZIP z dwuklikiem CMD, zachowaniem presetów, backupem i rollbackiem; panel bez wpisywania klucza.
- [x] USB/BT MIDI relay i MIDI Learn; dodany licznik oraz ostatni rzeczywiście odebrany komunikat CC w panelu, aby odróżnić brak MIDI od błędnego przypisania.
- [x] Właściciel potwierdził 29.10.2026: Ampero już działa i przełącza efekty na jego instalacji. Nie przypisywać tego potwierdzenia nowemu preview 5 bez instalacji.
- [ ] Automatyczny kolektor sprzętu, opóźnień i stanu usług podczas instalacji z usuwaniem sekretów.
- [ ] Pomiar round-trip na fizycznej pętli audio; oddzielić od latencji obliczonej z bufora i algorytmów.
- [ ] AUTO LATENCY TUNE na wspieranym urządzeniu, z wyciszeniem podczas zmian i zachowaniem bezpiecznej konfiguracji.
- [ ] Uniwersalny manager USB Audio; obecny instalator HeadRush jest celowo dopasowany do istniejącego bassambient.
- [ ] Graficzny EXE Windows VST3 i graficzny EXE instalatora RPi oraz pakiet DEB ARM64; obecne instalatory to zweryfikowane ZIP + CMD i TAR.GZ.
- [ ] Fizyczny odsłuch i pomiary CPU/xrun przy reprezentatywnym łańcuchu, w tym Cloud i Tape Warm.

Nie oznaczać żadnego z otwartych punktów jako ukończony bez odpowiednich wyników.
