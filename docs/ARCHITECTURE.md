# Architektura 0.1.0-preview.1

`multifx_dsp` nie linkuje GUI, JACK, ALSA ani VST3. Adaptery JA z JerzyAudioPlatform służą do natywnego wywoływania kopii oryginalnych DSP. Wszystkie instancje/bufory są przygotowywane poza callbackiem. Dwa zestawy instancji silników delay zachowują niezależny stan A/B. Nie są to hostowane wtyczki ani most VST→RPi.

Pięć bloków: Analog, Modulation, MD, Reverb, IR. Kolejność reprezentuje indeks jednej ze 120 permutacji. Zmiany porządku/dyskretnych ustawień są przygotowywane podczas zejścia do opóźnionej ścieżki suchej; DSP pracuje nadal, po przełączeniu następuje powrót do wet. Wewnętrzna kolejność CPD→Tape→Drive→Comp jest stała. Bypass ma wygładzanie i stałą całkowitą latencję 4240 próbek, również przy bypassie bloku modulacji. Oryginalny AttackDecay wnosi 2064 próbki, Atomic 2048, a IR 128.

Konwolucja: partition128 / FFT256, do128 partycji, dwa kanały. Trzy banki niezmiennych widm, atomowa publikacja i ochrona odczytywanego/starego banku. Producent jest jeden, poza audio. Podmiana wykorzystuje crossfade128 próbek. Bufor historii i FFT są stałe. Maksymalny IR16384 próbek jest limitem zasobów, nie ukrytą obietnicą długich reverb IR. Resampling odbywa się przy imporcie/przywracaniu stanu, nie w audio.

Model parametrów w `core/Model.h`; `docs/PARAMETERS.json` zawiera mapę. ID VST to utrwalone indeksy deklaracji; nie wolno przestawiać istniejących wpisów. Stan zapisuje hashe stabilnych kluczy, wartości normalizowane, nazwę/rate i próbki IR oraz CRC32. Dodawanie nowych kluczy wykorzystuje wartości domyślne; nieznane klucze są pomijane. CC maps mają własne trwałe parametry. ID klas VST są stałe w Plugin.h. Factory programs są zmianami ustawień efektu, nigdy sekwencerem ani źródłem automatycznych nut.

VST3: automatyzacja dzieli blok na odcinki na granicach punktów hosta; obsługiwane bufory32/64bit z konwersją do float32. Tempo/transport trafiają do natywnych procesorów. Metering wykorzystuje atomiki; GUI odpytuje poza callbackiem przez IMessage. GDI rysuje w buforze i skaluje programowo.

RPi: JACK jest ładowany dynamicznie, osobna warstwa control działa poza audio. Autołączenie opiera się na wykrytych portach fizycznych i MIDI; nie zgaduje interfejsu ALSA. RPC pozostaje loopback, panel z tokenem wystawia wyłącznie dozwolone operacje. BLE GATT i BLE MIDI używają tego samego RPC; szyfrowanie/parowanie wymuszają flagi BlueZ. Dysk i TCP nie występują w callbacku. GPIO wymaga osobnego potwierdzonego pliku konfiguracji i przeglądu elektrycznego.

Ochrona końcowa przed NaN/Inf i nieograniczonym poziomem nie jest obietnicą transparentnej pracy przy dowolnym zestawie skrajnych ustawień. Limiter końca Analog jest sample-peak -1dBFS; późniejsze bloki mogą zmienić poziom. Dla ostatecznego master limitera/true-peak potrzebna byłaby osobna implementacja i testy, nie deklaracja marketingowa.

Granice obecnego preview: brak zmierzonej kalibracji obwodowej i testów odsłuchowych na sprzęcie, brak automatycznego hotspotu/kreatora WWW do konfiguracji ALSA, zewnętrzny Return AttackDecay nie ma osobnej magistrali sidechain w tym adapterze, brak integracji nieznanych przycisków LCD. BLE/iOS offline jest klientem i API do walidacji sprzętowej. Szczegóły STATUS.md.
