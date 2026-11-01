# Jerzy MultiFX VST3 Windows — preview 22

W panelu FX kontrolki są podzielone na czytelne strony, a wybór bloku, efektu i parametrów jest opisany w oknie. Listy pokazują aktualny wybór. Kolorystyka MultiFX pozostała; czcionki systemowe i wektorowe elementy rysowane są w docelowej rozdzielczości okna. DSP, MIDI i identyfikatory parametrów bez zmian. Kod: a27671fae6367ba4b7ac526743245aa115a466c3.

Testy lokalne: Windows x64 cross-build; prawdziwy DLL VST3 uruchomiony pod Wine; GUI smoke (trzy cykle, strony i widoczność list), sygnał i PDC PASS; Steinberg validator kompletnego bundle 47/47 PASS; natywny DSP/RPi 3/3 PASS; zawartość ZIP i SHA256 PASS. Bez fizycznego testu w FL Studio i na Raspberry Pi.

SHA256 ZIP: 7fb8c9ef67587ac75eccabb28d5bdfc59ccd40804262c21290a982bcf8c7ddf9
