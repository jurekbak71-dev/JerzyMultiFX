# Zgodność USB Audio

Bieżący instalator `ZAINSTALUJ-NA-RASPBERRY.cmd` jest przeznaczony dla istniejącego `bassambient` z Debianem 13 ARM64, użytkownikiem `jerzy`, HeadRush Core i działającym JACK. Zachowuje jego konfigurację; wymaga obecnych portów system:capture_1/2 i playback_1/2. Inny interfejs nie jest automatycznie uznany za zgodny.

Nowe raporty instalacyjne zapisują listę ALSA, USB i MIDI, model/kernel i stan usług. Nie obejmują szczegółowych deskryptorów izochronicznych ani pomiaru hardware latency. Uniwersalny manager i test rzeczywistego odłączenia USB pozostają otwarte w TASKS.md.
