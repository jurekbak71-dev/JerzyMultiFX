# patchOS / Patchbox OS a tor HeadRush

Sprawdzone źródła:

- [elektrofon/patchOS README](https://github.com/elektrofon/patchOS/blob/master/README.md): Pi 4, interfejsy Expert Sleepers ES-8/ES-9, JACK + JackTrip do sieciowego przesyłania czterech kanałów. Nie jest to host multiefektu ani konfiguracja HeadRush. Repozytorium MIT; kodu nie kopiowano.
- [patchOS jack.service](https://github.com/elektrofon/patchOS/blob/master/etc/systemd/system/jack.service): osobny JACK z `LimitRTPRIO=95`, `LimitMEMLOCK=infinity` i konfiguracją urządzenia z pliku. Autorzy komentują, że szybkie ponawianie bez podłączonego USB może tworzyć pętlę uruchamiania.
- [MODEP mod-host.service](https://github.com/BlokasLabs/modep-debs/blob/master/modep-mod-host/modep-mod-host-1.13.0/debian/modep-mod-host.service): host efektów po JACK, osobne limity realtime, restart z odstępem 2 s. Patchbox OS buduje dedykowany system; jego ustawienia Pisound nie są ustawieniami HeadRush.

JerzyMultiFX już oddziela JACK, rdzeń DSP i panel. Dedykowany wariant ma limity realtime oraz zapisany wybór urządzenia ALSA. Dla niego dodano `RestartSec=5`, aby brak USB nie wywoływał natychmiastowej pętli restartów. Aktualizator `bassambient` pozostawia istniejący `jerzy-jack.service`, jego sample rate, okres i routing bez zmian; tej poprawki nie przypisujemy działającej instalacji HeadRush.

Nie ma podstaw, by samo skopiowanie jednostki systemd z patchOS poprawiło brzmienie. O różnicy między VST i Pi rozstrzyga porównanie identycznego presetu i poziomów wejścia/wyjścia, sample rate, CPU/xrun oraz rzeczywistego round-trip na HeadRush. Automatyczny raport przed/po już zbiera część danych; fizyczny pomiar i odsłuch pozostają otwarte.

Preview 11 dodaje do automatycznego raportu graf połączeń `jack_lsp -c` oraz zakresy latencji portów `jack_lsp -l`. Te dane ułatwiają wskazanie błędnego routingu w działającym torze HeadRush; wartości portów nie są pomiarem całego opóźnienia instrument → głośnik.
