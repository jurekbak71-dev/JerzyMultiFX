# 0.1.0-preview.19

- Attack/Decay MONO używa 16-próbkowego toru; POLY zachowuje 2064 próbki. Tor MONO omija zbędne oczekiwanie na okno FFT, zachowując te same detektor, obwiednię, kompresor i fuzz. Wtyczka VST3 raportuje zmienioną latencję hostowi.
- Edytor VST3 ma nowy ciemny układ z pionową listą pięciu bloków, dwoma bankami DELAY w nagłówku, czytelnym ON/BYPASS, suwakami wspólnych parametrów DELAY i wskaźnikami IN/OUT.
- Atomic Cluster nadal ma własne 2048 próbek opóźnienia analizy. Rzeczywista latencja interfejsu USB/JACK na Pi wymaga pomiaru na urządzeniu.

# 0.1.0-preview.18

- Panel RPi pokazuje schematy Hotone Ampero Control z pedałem ekspresji i WORLDE EASYCONTROL.9; można rozpoznać komunikat każdej kontrolki oraz przypisać CC do parametru bieżącego presetu.
- Jedno potwierdzone czyszczenie usuwa własne mapy MIDI z bieżącego brzmienia i zapisanych presetów, odłącza sloty PC, zachowuje brzmienia, IR i połączenia urządzeń.

# 0.1.0-preview.11

- Raport RPi przed/po instalacji zawiera połączenia portów JACK i zakresy latencji portów. Ułatwia porównanie działającego toru HeadRush z patchOS bez przestawiania audio.

## 0.1.0-preview.10

- Publikator Windows odczytuje odpowiedź GitHub CLI jako UTF-8 także na runnerze z kodowaniem cp1252. Preview 9 zbudowało VST3, ale jego upload przerwał się przy polskim opisie Release.

## 0.1.0-preview.9

- Dodatkowy pakiet Debian ARM64 z natywnym programem, pełnym payloadem z TAR.GZ i poleceniem `jerzy-multifx-setup`. Instalacja DEB nie przestawia audio automatycznie; konfiguracja świadomie korzysta z dotychczasowego instalatora z backupem. Poprawiono uprawnienia katalogu głównego archiwum po kontroli opublikowanego preview 8.

## 0.1.0-preview.7 — raport sprzętu RPi

- Aktualizator zapisuje diagnostykę Pi przed i po instalacji: ALSA/USB/MIDI, model, kernel, usługi, xruns/CPU i dane MultiFX. Dane są w prywatnym katalogu instalacji; token, środowisko i logi są pomijane.
- Raport odróżnia opóźnienie algorytmów i okres JACK od niezmierzonego round-trip. Instalator nie emituje sam sygnału testowego na HeadRush.
- Wszystkie funkcje preview 6, w tym wspólne mapowanie MIDI DELAY i mapy CC zapisane w globalnych presetach, pozostają.

## 0.1.0-preview.6 — wspólne MIDI DELAY i mapy w presetach

- Osobne dla DELAY A i DELAY B wspólne MIDI Learn TIME, FEEDBACK i poziomu efektu/MIX. Jedna mapa działa po zmianie Digital/MX/Memory/Reel/Oil; aktywny drugi silnik MX dostaje ten sam CC.
- Każdy globalny preset program0–program127 zapisuje pełny stan oraz własną mapę CC. Program Change wczytuje dany slot; niewypełnione 0–7 wybierają fabryczne brzmienie z fabryczną mapą.
- Przy włączonym sync TIME jest określany przez tempo/podział; ruch gałki wolnego czasu nie zmienia słyszalnego delayu do wyłączenia sync.
- Nie zmieniono identyfikatorów parametrów ani formatu `.jmfx`.

## 0.1.0-preview.5 — diagnostyka MIDI RPi

- Panel MIDI pokazuje, czy rdzeń faktycznie odebrał komunikat, numer CC, wartość 0/127 i przypisany efekt.
- Zachowuje poprawki DSP, GUI, MIDI, autostartu i instalatora preview 4.
- To wydanie diagnostyczne; fizycznego Ampero/HeadRush nie zweryfikowano.

## 0.1.0-preview.4 — RPi

- Czytelne ON/OFF: zielone WŁĄCZONY, czerwone WYŁĄCZONY/BYPASS; stan MIDI i natychmiastowa reakcja panelu, wysyłanie zmian podczas ruchu gałek.
- Listy/przełączniki dla trybów; MIDI Learn także PREAMP/TD/DRIVE/COMP i master. CC Toggle 0/127 dla bypassów.
- Tape Warm/mechanika, nowa dyfuzja Cloud, korekta Decay i Shimmer, niezależne od sample rate wygładzanie COMP/Digital, optymalizacja Memory.
- Autostart zachowujący brzmienie, restart USB/core po restarcie JACK, automatyczne odzyskiwanie radia Bluetooth, backup/rollback.
- Autosave poza kontrolą/audio z ograniczeniem zapisów SD, BPM i mono/stereo, import pełnego presetu VST. Panel bez wpisywania klucza.
- Szczegóły i ograniczenia: docs/DSP_REVIEW_PREVIEW4_PL.md oraz STATUS.md. Fizyczny odbiór sprzętu pozostaje wymagany.

## 0.1.0-preview.3

- Windows: studio rack with walnut cheeks, brushed steel/aluminium, ebonite knobs, toggle switches and independent IN/OUT peak VU needles; scrollable controls cannot cover the fixed effect switches.
- RPi: panel opens automatically with a same-origin HttpOnly session; no key entry. Installer enables boot services and preserves the complete last sound/MIDI state across updates.
- Explicit MIDI Learn for all web ON/OFF switches, sections, banks, blocks and master; Windows right-click Learn also covers block and master bypass.
- Physical FL Studio/HeadRush validation remains pending.

# Changelog

## 0.1.0-preview.2 — 2026-10-27

Przebudowane strony VST/RPi, lista silników MD, osobne efekty/bypassy, działający import IR, dynamiczne PDC, ograniczenie pracy i pamięci pomijanych efektów, FIR DRIVE, oryginalne modele Muff, Return/expression Attack, Settings/routing/bufor i nowy transport Bluetooth/USB MIDI. Instalatory dwuklikiem dla VST i aktualizacji bassambient. Szczegóły testów i ograniczeń w STATUS.md.

## 0.1.0-preview.1 — 2026-10-27

Pierwszy wspólny silnik pięciu bloków, natywne oryginalne DSP, VST3 Windows x64/GDI, host ARM64/JACK, pełny stan z IR/MIDI, kontrola HTTP/BLE i logika LCD. Testy natywne, Wine, QEMU i JACK dummy. Wydanie rozwojowe; fizyczny Windows/FL/RPi/BLE/LCD nie zostały potwierdzone. Braki i ograniczenia w STATUS.md.
