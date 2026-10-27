# Jerzy MultiFX — status

**0.1.0-preview.1 — IMPLEMENTATION IN PROGRESS, nie stable.**

## Wykonano
- Rozpakowano 26 referencji. Przejrzano TS808, DS-2, COMP i LCD; DM-2 używany wyłącznie jako referencja delay.
- Wspólna biblioteka DSP C++20: pięć przestawialnych bloków, oryginalne silniki i pełne parametry, A/B delay, FDN reverb, partycjonowana konwolucja.
- Pierwsza kompilacja rzeczywistego VST3 PE32+ Windows x64 oraz ELF ARM64 zakończona.
- Pierwsze testy natywne i ARM64/QEMU przeszły: IR tożsamościowy, latencja128, stereo44.1/48/96kHz, bufory32–1024.
- Edytor Windows GDI, automatyzacja, MIDI Learn, stan z CRC i osadzonym IR; host RPi JACK, panel HTTP z tokenem, BLE GATT/MIDI i logika LCD w implementacji.

## Dalsze potwierdzone wyniki
- 47/47 kontroli walidatora Windows VST3 na poprawnym bundle pod Wine.
- Rzeczywisty edytor DLL: attach, regulator, bypass, MIDI Learn, reorder, resize, screenshot, close — PASS.
- Host JACK dummy: sygnał audio, reakcja gain, MIDI Learn CC74 — PASS; 0 xrunów w ostatnim krótkim przebiegu.
- HTTP/control natywnie i ARM64/QEMU: auth, 918 parametrów w pierwszym przebiegu, zapis/odczyt, stereo IR44100→48000 i walidacja danych — PASS. Dodano następnie osobny Factory Program (919 parametrów) i trwały autostart stanu; ponowna walidacja w toku.
- Cztery testy logiki BLE/LCD — PASS; żadnej deklaracji testu radia ani fizycznego LCD.

## W toku
- Rozszerzenie testów RT/stanu/routingu/GUI i uruchomienie walidatora VST3 pod Wine.
- Testy integracyjne hosta, panelu, usług, BLE i LCD bez sprzętu.
- Paczki, sumy kontrolne, kompletny kod źródłowy i polskie instrukcje.

## Ograniczenia
- Fizyczny RPi/interfejs audio, FL Studio, BLE/Bluefy/iOS, LCD: **AWAITING HARDWARE VALIDATION**.
- Brak opublikowanego GitHub Release/Actions; nie podajemy fikcyjnych linków.
- Kod źródłowy innych repozytoriów nie jest modyfikowany; kopie i ich SHA w docs/source-manifest.json.
- Ilustracje referencyjne nie będą redystrybuowane w paczkach. Niektóre oryginalne źródła nie mają jawnej licencji — publiczna dystrybucja wymaga uporządkowania praw autora.
