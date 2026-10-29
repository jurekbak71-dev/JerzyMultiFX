> Od preview 3 panel otwiera się automatycznie pod http://bassambient.local:8097 — nie wpisujesz klucza. Starsze instrukcje tokenu dotyczą preview 1/2. Po instalacji MultiFX startuje z Pi i odtwarza ostatnie brzmienie. MIDI Learn przy ON/OFF obejmuje także PREAMP, TAPE DRIVE, DRIVE i COMP. Dla przełączania użyj CC naprzemiennie 0/127. Panel jest przeznaczony dla zaufanej sieci lokalnej.

# Instalacja Raspberry Pi ARM64

Od preview 9 dostępny jest również `JerzyMultiFX-RPi-aarch64-v…deb` dla Debian/Raspberry Pi OS 13 ARM64. Na **oddzielnym Pi** można wykonać `sudo dpkg -i JerzyMultiFX-RPi-aarch64-v…deb`, a następnie `sudo jerzy-multifx-setup --enable --device hw:NAZWA`. Samo `dpkg -i` jedynie rozpakowuje sprawdzone pliki do `/usr/share/jerzy-multifx` i dodaje polecenie konfiguracji; nie uruchamia usług, nie zmienia JACK ani konfiguracji audio. `sudo jerzy-multifx-setup --help` pokazuje opcje. Dla istniejącego `bassambient` z HeadRush użyj instalatora ZIP, który zachowuje działający JACK i ma własne cofnięcie zmian.

Docelowy system tej paczki: Debian 13 / Raspberry Pi OS 64-bit z glibc >= 2.41, architektura `aarch64`. Nie jest to paczka ARM32 ani Windows. Typowy kandydat: Raspberry Pi 4/5; obciążenie i opóźnienie muszą być zmierzone na fizycznym urządzeniu. Testy QEMU nie potwierdzają czasu rzeczywistego.

1. Przygotuj **oddzielną** kartę w Raspberry Pi Imager; nie nadpisuj działającej karty MultiFX. Skonfiguruj własną sieć, SSH i mocne hasło. Żaden skrypt z repozytorium nie formatuje karty.
2. Podłącz interfejs audio, gitarę do jego wejścia i wyjście do monitora/wzmacniacza z bezpiecznym poziomem. Nie podłączaj sygnału głośnikowego do wejścia liniowego. Model i numer urządzenia nie są zakładane.
3. Przez SSH sprawdź `uname -m`, `getconf GNU_LIBC_VERSION`, `arecord -l`, `aplay -l`, `lsusb`. Wybierz identyfikator ALSA, np. stabilną nazwę `hw:NAZWA`, a nie losowo `hw:0,0`.
4. Prześlij i rozpakuj `JerzyMultiFX-RPi-aarch64-v0.1.0-preview.1.tar.gz`. Sprawdź `sha256sum -c SHA256SUMS` w katalogu paczki.
5. `sudo bash scripts/install.sh` instaluje pliki i zależności, tworzy kopię istniejącej wersji oraz konto usługi; domyślnie nie uruchamia audio. `sudo bash scripts/install.sh --enable --device hw:NAZWA` świadomie uruchamia dedykowany JACK i audio/web. Jeśli wykryje już działający JACK, zatrzymuje instalację przed zmianą usług — integrację z istniejącą platformą wykonaj ręcznie.
6. Dedykowany JACK: 48 kHz, bufor 128, 3 okresy, bez wymuszania realtime. Parametry urządzenia w `/etc/jerzy-multifx/audio.env`; inne częstotliwości/bufory zmienia się po testach i edycji dedykowanej jednostki systemd. Ustawienie realtime i uprawnień pamięci wymaga pomiaru; pakiet nie obiecuje konkretnej latencji sprzętowej.
7. Sprawdź `sudo bash scripts/diagnostics.sh`, statusy usług i dziennik `journalctl -u jerzy-multifx -b`. Panel na porcie 8097 otwiera się automatycznie w lokalnej sieci, bez wpisywania klucza.
8. JACK automatycznie łączy wykryte fizyczne porty audio i porty MIDI, bez zakładania modelu USB. Dla ALSA MIDI może być potrzebny standardowy most `a2jmidid`; sprawdź routing, channel i MIDI Learn. BLE-MIDI jest odrębną usługą. Mapy zapisują się do `last-state.jmfx` poza wątkiem audio i wracają po starcie. Presety Program Change mają nazwy `program0.jmfx`–`program127.jmfx` w katalogu `presets`.
9. IR przez panel Wi-Fi importuje się do kontrolowanej biblioteki; USB: skopiuj WAV do biblioteki IR, a następnie wybierz go kontrolerem. BLE może wybierać istniejący IR, nie wysyła dużych WAV. Limit: 16384 próbek po konwersji sample rate.

## Backup, aktualizacja, rollback

Dane: `/var/lib/jerzy-multifx`; konfiguracja: `/etc/jerzy-multifx`; pliki aplikacji: `/opt/jerzy-multifx`. Przed aktualizacją wykonaj osobną kopię danych. Instalator zapisuje poprzednią aplikację i jednostki w `/var/backups/jerzy-multifx/…/previous.tar.gz`. `sudo bash scripts/rollback.sh /var/backups/jerzy-multifx/…/previous.tar.gz` przywraca pliki, po czym trzeba sprawdzić konfigurację i uruchomić własne usługi. Nie zmienia innych platform/JACK. `sudo bash scripts/uninstall.sh` wyłącza wyłącznie jednostki MultiFX i zachowuje dane/konfigurację.

## LCD 1602A

Zdjęcie pokazuje QAPASS 1602A, 16 pinów, bez I²C. Sterownik `rpi/lcd.py` realizuje protokół HD44780 w 4-bitach. Typowy układ sygnałów RS/E/D4–D7 **nie jest potwierdzonym pinoutem konkretnego modułu**. Przed zasileniem sprawdź datasheet, masę, VDD, kontrast, prąd podświetlenia, RW i progi logiczne. RW musi pozostać LOW w trybie write-only; nie wprowadzaj 5 V na GPIO RPi. W razie potrzeby użyj właściwego level shiftera. Nie podajemy niezweryfikowanego schematu połączeń jako gotowego do wykonania.

Po przeglądzie elektrycznym przygotuj plik JSON z `electrical_review_complete: true`, `chip` i sześcioma zweryfikowanymi offsetami GPIO `[RS,E,D4,D5,D6,D7]`, zainstaluj python3-libgpiod v2 i uruchom `rpi/lcd_service.py --config …`. Domyślny ekran pokazuje audio/output/xrun. Nawigacja przyciskami/enkoderem wymaga uzgodnienia faktycznie podłączonych elementów i dalszej integracji.
