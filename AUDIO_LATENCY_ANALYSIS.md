# Latencja audio — wartości o różnym znaczeniu

Tor: instrument → ADC HeadRush → USB ALSA/JACK capture → DSP MultiFX → JACK/USB playback → DAC HeadRush. Każdy z tych etapów dodaje opóźnienie. Sam licznik `latencySamples` w MultiFX obejmuje opóźnienie algorytmów w aktualnym torze, np. Attack Decay 2064 próbek lub Atomic 2048 próbek. Nie obejmuje przetworników, sterownika USB ani całego buforowania JACK.

Raport automatyczny `diagnostyka-przed.json` i `diagnostyka-po.json` podaje oddzielnie:
- `algorithm_ms`: obliczone `latencySamples / sampleRate` dla aktualnego toru;
- `one_period_ms`: obliczony czas jednego okresu JACK `bufferSamples / sampleRate`;
- `round_trip_measured_ms`: `null`, dopóki nie zostanie przeprowadzony pomiar fizycznego loopback.

Nie wolno sumować tych dwóch obliczeń i nazywać wyniku zmierzonym opóźnieniem. Rzeczywisty round-trip należy zmierzyć sygnałem podanym z wyjścia na wejście interfejsu, przy kontrolowanym poziomie, bez podwójnego monitoringu i po uwzględnieniu trasy sprzętowej. Instalator nie wysyła automatycznie sygnału testowego na głośniki lub HeadRush. AUTO LATENCY TUNE także pozostaje osobnym zadaniem: wymaga bezpiecznego wyciszenia, prób stabilności XRUN i cofnięcia nieudanej konfiguracji.

Zebrane pliki nie zawierają tokenu, zmiennych środowiska ani logów systemowych. Zawierają nazwy urządzeń USB i portów MIDI, więc użytkownik decyduje o ich udostępnieniu.
