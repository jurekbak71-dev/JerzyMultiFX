# Instalacja VST3 Windows x64

1. Zamknij FL Studio lub inny program muzyczny.
2. Rozpakuj cały ZIP z paczką VST3.
3. Kliknij dwa razy `ZAINSTALUJ-VST3.cmd`. Zatwierdź okno Windows proszące o instalację w folderze wtyczek.
4. Sprawdź komunikat `Zainstalowano`, numer wersji i potwierdzoną sumę SHA256.
5. Otwórz DAW, wykonaj pełne wyszukiwanie wtyczek i wybierz **Jerzy MultiFX**. Numer aktualnej wersji jest widoczny w nagłówku własnego GUI.

Instalator zachowuje poprzednią wersję w `C:\ProgramData\JerzyMultiFX\kopie`.
Potwierdzenie dokładnie zainstalowanej wersji zapisuje w `C:\Program Files\Common Files\VST3\Jerzy MultiFX.vst3\ZAINSTALOWANA-WERSJA.txt`.
Własny panel wtyczki ma pięć bloków, osobne strony efektów i listę silników MD.
Jeśli DAW pokazuje ogólną listę parametrów, przełącz widok wtyczki na jej własny edytor (GUI).
Bufor interfejsu audio ustawia się w DAW. Czysty tor ma 0 próbek dodatkowego opóźnienia DSP; aktywne Attack Decay, Atomic, DRIVE i IR zgłaszają własne opóźnienia hostowi.

## Instalacja ręczna

Skopiuj cały folder `Jerzy MultiFX.vst3` do `C:\Program Files\Common Files\VST3`, następnie ponownie wyszukaj wtyczki w DAW. Zachowaj cały bundle, nie wyjmuj samego pliku z `Contents`.

## Stan i testy

To preview. Testy kodu, walidator VST3 i test rzeczywistego edytora nie zastępują odsłuchu w Twoim DAW. Pełne wyniki aktualnego wydania są w `STATUS.md` oraz w odnośniku Actions na stronie Release. Nie deklarujemy przeprowadzonego testu FL Studio na Twoim sprzęcie.
