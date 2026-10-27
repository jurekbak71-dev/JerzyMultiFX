> Od preview 3 panel otwiera się automatycznie pod http://bassambient.local:8097 — nie wpisujesz klucza. Starsze instrukcje tokenu dotyczą preview 1/2. Po instalacji MultiFX startuje z Pi i odtwarza ostatnie brzmienie. MIDI Learn przy ON/OFF obejmuje także PREAMP, TAPE DRIVE, DRIVE i COMP. Dla przełączania użyj CC naprzemiennie 0/127. Panel jest przeznaczony dla zaufanej sieci lokalnej.

# Połączenie z bassambient / HeadRush

Panel: **http://bassambient.local:8097**. Komputer lub telefon ma być w tej samej sieci domowej. Panel otwiera się automatycznie, bez wpisywania klucza. Nie trzeba internetu do obsługi panelu.

W panelu otwórz **MIDI i Bluetooth**:

- Kontroler MIDI Bluetooth: wybierz wyszukiwanie, urządzenie, parowanie i połączenie MIDI. Jeżeli pojawi się kod, potwierdź go. Panel pokazuje połączenie i odbierane komunikaty.
- USB MIDI: podłącz kontroler do USB Raspberry. W sekcji USB MIDI wybierz jego port. Prawy klik regulatora VST lub przycisk MIDI Learn na stronie RPi uzbraja przypisanie CC.
- Telefon przez Bluetooth: osobna sekcja połączenia sieciowego PAN. Połączenie MIDI nie otwiera strony panelu. Telefon musi obsługiwać sieć Bluetooth; zależy to od systemu telefonu. Najprostszy dostęp do panelu pozostaje przez domowe Wi-Fi.

Aktualizator HeadRush używa istniejącego brokera `jerzy-audio-hardware.service`, zachowując Wi-Fi, parowania i systemowy JACK. Nowy natywny przekaźnik MIDI kieruje zdarzenia do MultiFX zamiast nieistniejących portów starego programu. Odbiór USB zapewnia a2jmidid; nie uruchamiamy drugiej kopii wykrytego mostu. Rozłączanie, ponowne łączenie i wybór źródeł są w panelu.

Repozytorium zawiera również eksperymentalne usługi GATT dla innych instalacji. Aktualny panel HeadRush używa HTTP; nie deklarujemy obsługi GUI offline przez samo BLE ani zgodności Bluefy/iOS. Radio Bluetooth, telefon PAN i kontrolery USB wymagają fizycznych testów preview 2. Automatycznie sprawdzono parsery, transport Unix/JACK i rzeczywiste sterowanie parametrem przez MIDI, bez radia.
