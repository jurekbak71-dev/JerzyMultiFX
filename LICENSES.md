# Licencje i dystrybucja

Nowy kod projektu jest udostępniany na AGPL-3.0 zgodnie z plikiem LICENSE. Obejmuje obowiązek udostępnienia odpowiadających źródeł przy dystrybucji oraz użytkownikom sieciowym objętym AGPL. Pakiety zawierają `source.tar.gz` i noty. Nie dołączają prywatnych konfiguracji, tokenów ani obrazów referencyjnych.

ReelEcho i OilDrumEcho: AGPL-3.0, oryginalne LICENSE zachowane przy kopiach. BigMuff: GPL-3.0; połączenie z AGPL-3.0 należy rozpatrywać zgodnie z §13 GPLv3/AGPLv3; oryginalny tekst pozostaje zachowany. Pozostałe wykorzystane DSP nie mają jawnego samodzielnego LICENSE w checkoutach — wykorzystanie na żądanie właściciela w prywatnym projekcie jest opisane w SOURCE_AUDIT. Nie nadajemy cudzym źródłom arbitralnie nowej licencji. **Publiczny Release pozostaje zablokowany do jednoznacznego ustalenia praw do tych komponentów.**

Steinberg VST3 SDK 3.8.1 (SHA3cdf9ca…): MIT, noty SDK i modułów w `dependencies/vst3sdk` odpowiadających źródeł. Nie korzystamy z VSTGUI ani JUCE w nowym silniku/front-endzie. RPi dynamicznie korzysta z systemowej biblioteki JACK, nie statycznie dołączonej platformy audio. Python/D-Bus/GObject/BlueZ/libgpiod są zależnościami systemowymi z ich własnymi licencjami, instalowanymi z podpisanych repozytoriów.

Adaptery C ABI i wzorzec JACK/probe wywodzą się z JerzyAudioPlatform; lista SHA i plików znajduje się w docs/SOURCE_AUDIT.md. Kod źródłowy repozytoriów referencyjnych pozostał niezmieniony.

Schematy TS808/DS-1/DS-2/COMP/DM-2 są materiałami badawczymi. Posiadanie pliku nie oznacza prawa do redystrybucji ilustracji; zostały wykluczone z archiwów produktu i odpowiadających źródeł.

Lokalny cross-build używa MinGW-w64 oraz GCC/libstdc++/libgcc i odpowiednich wyjątków bibliotek wykonawczych; są to standardowe elementy toolchaina/System Libraries. Paczka nie zawiera Wine, QEMU ani narzędzi deweloperskich chmury. Wine/QEMU służyły wyłącznie do walidacji i nie są backendem audio produktu.
