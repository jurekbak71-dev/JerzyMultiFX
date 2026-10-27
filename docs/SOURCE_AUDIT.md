# Audyt źródeł

Pełne SHA i mapowanie plików: [source-manifest.json](source-manifest.json).

- **JerzySmallCloneChorus** `0aa20a4f0584c674065bb191b45b73d97df5772d` → `vendor/clone`: No standalone license in source checkout; integration requested by owner, redistribution requires confirmation of rights
- **JerzyAtomicClusterVST3** `ae82eac1976dc703600fb78102bf2d8ae9bca2c6` → `vendor/atomic`: No standalone license in source checkout; integration requested by owner, redistribution requires confirmation of rights
- **JerzyTapeDriveVST3** `3ee7f10f69b9488f5cc10087be1a54df097ef789` → `vendor/tape`: No standalone license in source checkout; integration requested by owner, redistribution requires confirmation of rights
- **JerzyMXAnalogDelayVST3** `aed456883fce4448afa9767852ff9eb82d147dca` → `vendor/mx`: No standalone license in source checkout; integration requested by owner, redistribution requires confirmation of rights
- **JerzyMemoryDelay** `3a3c4238463688cc6f5499b30abb6a09e4c1f68f` → `vendor/memory`: No standalone license in source checkout; integration requested by owner, redistribution requires confirmation of rights
- **JerzyAttackDecayVST3** `b998a1467437311d3361b3dc934d785e025d4c40` → `vendor/attack`: No standalone license in source checkout; integration requested by owner, redistribution requires confirmation of rights
- **JerzyReelEchoVST3** `b6d03eccfaadb931df81cb342f8f2a4af739203b` → `vendor/reel`: GNU AFFERO GENERAL PUBLIC LICENSE
- **JerzyOilDrumEcho** `f50340031f73afe01378adcbc82611a52645bc31` → `vendor/oil`: GNU AFFERO GENERAL PUBLIC LICENSE
- **JerzyBigMuffCollectionVST3** `ce8ca2a848a57c6e6a91a596e3cc0016e7a22554` → `vendor/muff`:                     GNU GENERAL PUBLIC LICENSE

Integracja bazuje na natywnych DSP, bez hostowania VST wewnątrz RPi. Adaptery C ABI pochodzą z JerzyAudioPlatform db7727ca8032b3c75e542fcf0d152533467f4619. Nowy frontend RPi wykorzystuje kontrakt JACK/MIDI i rozdzielenie audio/control tej platformy, bez jej starej listy efektów. JerzyRPiAudioLab e210a367d74a99ce7d9d1592f9f5ce97cf134e12 służy jako referencja sprzętowa.

TS808: filtrowane ujemne sprzężenie, przeciwstawne diody i podbicie środka; implementacja behawioralna, bez deklaracji identyczności obwodowej. DS-1/DS-2: osobna charakterystyka twardego clippingu i Turbo; DM-2 nie bierze udziału w DRIVE. COMP: własna obwiednia stereo z miękkim kolanem i dwoma czasami powrotu inspirowanymi OTA/optycznym zachowaniem.

Schematy w references nie są dołączane do paczek binarnych — prawa ilustracji nie zostały ustalone. Kopie kodu bez jawnej licencji pozostają wyraźnie oznaczone; publiczne wydanie wymaga uporządkowania praw. Nie wyciągamy uprawnień do cudzych plików ze zrzutów ekranów.

Preview 2: `vendor/muff/Circuit/AnalogCircuit.h` i `Catalog.h` są niezmienionymi kopiami oryginalnego solvera/katalogu z tego samego SHA BigMuff (GPL); sumy w source-manifest.json. Adapter `effects/Native.h` wybiera 6 lżejszych Legacy albo 14 oryginalnych modeli obwodowych, z oversamplingiem FIR 4×. Oryginalne repozytoria pozostają bez zmian. Analiza ergonomii i granic porównań: [ERGONOMIA_AUDIO_AUDYT_PL.md](ERGONOMIA_AUDIO_AUDYT_PL.md).
