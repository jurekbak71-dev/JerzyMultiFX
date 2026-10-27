# Testy wymagające sprzętu — jeszcze niewykonane

- Windows 10/11 x64 + FL Studio: skan, nowa instancja, 44,1/48/96 kHz, otwarcie/zamknięcie GUI, 80/100/125/150/200% DPI, zmiana kolejności, MIDI CC, zapis i ponowne otwarcie FLP, offline WAV kontra real-time, wiele instancji, długa praca bez zaniku audio.
- Raspberry Pi 4/5 ARM64 + faktyczny interfejs USB/I²S: identyfikacja ALSA, sample rate/bufory32–1024, capture→output, pomiar roundtrip latency, CPU/RAM/temperatury, xruny i długie obciążenie wszystkich modułów z dwoma delayami, Shimmer i IR; reboot i przywrócenie presetów/map.
- USB-MIDI: hotplug, footswitch, CC/expression, MIDI Learn, reconnect; unikać duplikowania wejścia przez równoczesny ALSA/USB/BLE routing.
- BLE: sparowany i niesparowany klient, szyfrowanie, odmowa nieuwierzytelnionego zapisu, reconnect, powiadomienia, BLE-MIDI i GUI równocześnie. iPhone/iPad Bluefy: faktycznie otwarte GUI offline przy wyłączonym Wi-Fi. Android Chrome: równoważny test.
- LCD: potwierdzić model/pinout/progi3,3V/5V, zasilanie/kontrast/podświetlenie, RW LOW i level shifting; następnie czasy inicjalizacji, 16×2, brak zakłóceń audio. Przyciski/enkoder dopiero po ustaleniu ich okablowania.
- Instalator: świeża karta Debian13 ARM64, istniejące dane i wcześniejsza wersja, brak naruszenia innej platformy, autostart, backup, rollback i uninstall zachowujący dane.

Status wszystkich powyższych: **AWAITING HARDWARE VALIDATION**. Wine i QEMU są testami programowymi/emulacją, nie testami sprzętu.
