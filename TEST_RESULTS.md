# Ostatnia weryfikacja

Preview 11: lokalne testy Python oraz natywny CI ARM64 i weryfikacja paczki
zostaną odnotowane po wykonaniu. Funkcja diagnostyczna nie zmienia toru audio.

Preview 7, źródło `e207a59703d984702258a567da6cff0512f28ae4`.
Natywny run ARM64: https://github.com/jurekbak71-dev/JerzyMultiFX/actions/runs/40837981375 — SUCCESS. CTest DSP 3/3, Python 30/30, Node 9/9, HTTP/core, JACK dummy/audio/MIDI, pakowanie i prywatność: PASS.
Po pobraniu assetów: zewnętrzne/wewnętrzne SHA256, ELF64 AArch64 obu programów, źródłowy SHA, identyczność ZIP instalatora z TAR.GZ i obecność automatycznego kolektora przed/po: PASS.

Niemierzone: fizyczne round-trip HeadRush, wydajność na fizycznym Pi, USB/BLE na nowej paczce i AUTO LATENCY TUNE. Wynik CI nie dowodzi ich działania.
