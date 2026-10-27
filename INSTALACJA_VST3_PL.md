# Instalacja VST3 Windows x64

1. Wybierz rzeczywistą paczkę `JerzyMultiFX-VST3-Windows-x64-v0.1.0-preview.1.zip` z katalogu `dist`/Artifacts. Sprawdź sumę SHA-256 względem `SHA256SUMS`. Wydanie preview wymaga testów w docelowym DAW.
2. Zamknij FL Studio. Jeśli masz starszą wersję Jerzy MultiFX, skopiuj jej folder jako backup.
3. Rozpakuj i skopiuj **cały folder** `Jerzy MultiFX.vst3` do `C:\Program Files\Common Files\VST3\`. Wewnątrz musi istnieć `Contents\x86_64-win\Jerzy MultiFX.vst3`; nie kopiuj tylko wewnętrznego DLL.
4. FL Studio → Options → Manage plugins → Find installed plugins / ponowny skan. Wstaw Jerzy MultiFX jako efekt miksera, podaj sygnał audio. Wtyczka nie uruchamia sekwencera ani własnych nut po starcie transportu.
5. Kafle u góry przeciąga się, aby zmienić kolejność pięciu bloków. Dwuklik kafla: bypass. Strony parametrów obejmują pełne oryginalne silniki. Klawisz Shift daje dokładniejszą regulację; Ctrl+kółko myszy — mały krok.
6. W FL przypisz port wejścia MIDI do instancji efektu. Prawy klik regulatora → porusz kontrolerem CC; przypisanie zapisuje się w stanie. Można też automatyzować parametry hosta. Nie obiecujemy obsługi sprzętowego MIDI bez sprawdzenia routingu FL.
7. LOAD IR WAV importuje IR; LOAD/SAVE PRESET operują na plikach `.jmfx`. IR i mapy CC są osadzane w stanie FLP i presetach. Zapisz FLP, zamknij i otwórz, wykonaj porównanie offline WAV z odtwarzaniem — to wymagana kontrola manualna.

## Czarne okno / troubleshooting

Renderer jest programowy GDI, bez akceleracji GPU. Sprawdź działanie w nowej instancji, skalę GUI 100% i ustawienia skalowania wrappera FL, następnie 80/125/150/200%. Zapisz log i wersję FL. Nie zmieniaj ID wtyczki, by obejść problem. Test Wine/Xvfb sprawdził rzeczywisty DLL, okno i interakcje; nie zastępuje fizycznego Windows/FL Studio.

Sample rate: 44,1/48/96 kHz w podstawowym zestawie testów, mono/stereo; wejście VST3 64-bit jest przeliczane do wspólnego silnika float32. Deklarowana latencja całego łańcucha: 4240 próbki (2064 z AttackDecay + 2048 z Atomic + 128 konwolucji), około 88,3 ms przy 48 kHz. To preview nie jest potwierdzonym rozwiązaniem low-latency do gry na żywo. Limiter jest sample-peak, ceiling -1 dBFS; nie jest to miernik ani limiter true-peak. W skrajnych ustawieniach innych bloków kontroluj poziom wyjściowy.

Odinstalowanie: zamknij DAW i przenieś tylko folder `Jerzy MultiFX.vst3` do backupu. Nie usuwaj projektów, IR ani presetów użytkownika.
