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
