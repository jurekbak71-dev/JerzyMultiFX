# Macierz walidacji

| Obszar | Test | Znaczenie / ograniczenie |
|---|---|---|
| DSP | CoreTests | IR impulsowy wielopartycyjny, latencja128, 120 permutacji, CRC/parametry/MIDI/IR, finite, bypass, zero alokacji w audio |
| Sample rate | 44100/48000/96000 | Stereo z antyfazą, bufory32/64/128/256/1024; mono w walidatorze VST |
| Windows | rzeczywisty PE32+ pod Wine + validator SDK | Nie jest testem FL Studio; natywny Windows CI oddzielnie |
| GUI | EditorSmoke, screenshot | attach, regulator, bypass, MIDI Learn, drag/reorder, 880×592/1280×850/1920×1275, detach; Direct2D/DirectWrite z tekstowym fallbackiem GDI pod Wine |
| ARM64 | ELF64 AArch64 pod QEMU | DSP i rzeczywisty HTTP/control; nie jest pomiarem realtime RPi |
| Audio | dummy JACK + JackProbe | Rzeczywista droga audio, zmiana gainu, USB MIDI CC74 Learn, xrun count |
| HTTP | integration.py | Auth, pełne parametry, presety, stereo IR44100→48000, odrzucenie traversal/zakresów |
| BLE | test_control.py | Parser MIDI/Program Change, paginacja<=512, walidacja; bez testu radia/parowania |
| LCD | test_control.py | Nibble/RS/E, adresy DDRAM, wiersze16; bez elektryki/GPIO |
| Paczki | package.py + odczyt ZIP/TAR | PE x64/ELF183, rzeczywiste pliki, źródła/noty, manifest SHA256 |

Przed instalacją na sprzęcie należy wykonać HARDWARE_TESTS_PL.md. Długotrwałe obciążenie termiczne i roundtrip latency nie są wywodzone z czasu wykonania QEMU.
