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
