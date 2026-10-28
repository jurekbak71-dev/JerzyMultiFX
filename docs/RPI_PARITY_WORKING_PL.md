# RPi — poprawki robocze względem sprawnego VST

**Na polecenie właściciela: bez kompilacji, Actions, publikacji i instalacji na urządzeniu.**

## Ustalenia z kodu i wyniku właściciela

VST i RPi wywołują ten sam `Engine::process`. Wspólne DSP, algorytmy, ID i domyślne wartości VST pozostają niezmienione. Samo współdzielenie kodu nie potwierdza identyczności odsłuchu: należy porównać ten sam preset/IR, sample rate, tempo i wejście/wyjście karty. RPi pobiera sample rate z JACK; VST pobiera również tempo/transport z DAW, przed poprawkami RPi używało stałego 120 BPM i `playing=false`. Dodano ustawienie/zapis BPM oraz odczyt aktywnego transportu JACK; nie uruchamiamy go automatycznie. Tego nie wolno nazywać potwierdzoną zgodnością wszystkich modeli.

Znalezione błędy panelu: debounce gałki odkładał wysyłanie aż do ustania ruchu; compact slider wysyłał dopiero przy puszczeniu; focused button był pomijany w odświeżaniu; kafle bloków nie aktualizowały się po MIDI; status master był napisem opisującym akcję, bez stanu/koloru; odpytanie co 500 ms dawało wyraźny lag. Starsza odpowiedź STATE mogła nadpisać nową lokalną zmianę. Część dyskretnych opcji (np. tryby TD, booleany Oil/Memory/MX) była opisana jako float.

Wynik właściciela jednoznacznie potwierdził obecność W-FADER w ALSA, most a2jmidid zakończony kodem 0 po restarcie JACK i brak restartu przy `on-failure`. Bluetooth: Powered=no / off-blocked, po unblock natychmiastowe Power On zwróciło Busy. Nie ma dowodu uszkodzenia kontrolera ani potwierdzenia odsłuchu poprawionej wersji.

## Zmiany w kodzie RPi

- Jednolity stan: WŁĄCZONY = zielone tło; WYŁĄCZONY / BYPASS = czerwone tło. Nazwa efektu oddzielona od stanu, aria-pressed odpowiada aktywności efektu, a nie surowemu bitowi bypass. Zmiana natychmiast lokalnie z oznaczeniem oczekiwania na zapis, wycofaniem przy błędzie i ochroną przed starym STATE.
- Gałki i suwaki wysyłają najnowszą wartość co 20 ms podczas ruchu; ostatnia po puszczeniu. Przełączniki nie przebudowują całego panelu. Odczyt MIDI/statusów co 100 ms, bez nakładania zapytań. HTTP keep-alive/TCP_NODELAY usuwa opóźnienie oddzielnego wysyłania nagłówków/body.
- Oddzielna tabela `rpi/ParameterUI.h`: przełączniki, nazwane listy i pola liczb całkowitych zamiast gałek opcji. Bazowa tabela VST nie jest zmieniana. Doprecyzowano m.in. TD, Memory, Reel, Oil, głowice MX; ciągły Memory Boost pozostaje regulacją ciągłą.
- Automatyczne kodowanie i zapis last-state przeniesione do osobnego, niskopriorytetowego wątku. Kolejka przechowuje ostatni snapshot, zapis jest atomowy, zamknięcie czeka na ostatni zapis. Audio nie korzysta z mutexa zapisującego.
- Ustawienie BPM 20–400 i zapamiętanie go po restarcie; odczyt pozycji/rolling transportu JACK zamiast stałego playing=false. Zgodność fazy nadal zależy od tej samej pozycji transportu i wymaga pomiaru.
- Jawna lista wejścia Stereo / Mono L / Mono R; mono powiela wybrany kanał jak wejście mono VST, bez sumowania i dodatkowego gainu. Domyślny routing stereo pozostaje zachowany, wybór jest zapamiętywany.
- Import pliku `.jmfx` z VST przez panel RPi wykorzystuje ten sam dekoder C++ i osadzony IR/mapy. Nie daje automatycznie tego samego poziomu fizycznego wejścia ani tempa DAW.
- USB: Restart=always, sprawdzanie obcego mostu dopiero po zatrzymaniu znanych starych usług (wcześniej wykryty stary most mógł zostać zatrzymany i zastąpić go już nie uruchamiano).
- Root-owned watchdog Bluetooth: zdrowe radio bez ingerencji; unblock z odczekaniem i ponawianiem Busy; reset BlueZ tylko gdy to konieczne, przywrócenie brokera w finally. Nie kasuje parowań/Wi-Fi/JACK. Systemd timer ponawia odzyskiwanie po starcie i późniejszych problemach. Instalacja zapisuje backup i dodaje odtworzenie do COFNIJ.sh. Fizyczna blokada radia jest zgłaszana, nie udawana jako naprawiona.
- Dedykowany wariant systemd JACK miał jawne `--no-realtime`: zmieniony na RT z limitami rtprio/memlock jak w hostach scenicznych. **Istniejący systemowy JACK HeadRush nie jest automatycznie przestawiany**. RPi STATE pokazuje rzeczywisty tryb RT.

## MODEP / Patchbox OS i patchOS

Odczytano źródłowy unit [MODEP mod-host](https://github.com/BlokasLabs/modep-debs/blob/master/modep-mod-host/modep-mod-host-1.13.0/debian/modep-mod-host.service): After/BindsTo JACK, LimitRTPRIO=95, LimitMEMLOCK=infinity, Restart=always. Użyte zasady: oddzielenie DSP od panelu i dysku, jawny backend RT i odzyskiwanie usług. Nie instalujemy innego OS ani konfiguracji Pisound na HeadRush.

[Patchbox OS](https://github.com/BlokasLabs/patchbox-os-gen) to dystrybucja Blokas z MODEP. Oddzielny projekt [elektrofon/patchOS](https://github.com/elektrofon/patchOS) opisuje streaming audio między syntezatorami z ES-8/ES-9; nie jest odpowiednikiem całego naszego multiefektu. Przeczytanie ich źródeł nie jest pomiarem wydajności na Pi właściciela.

## Granica obecnego etapu

Wykonano wyłącznie interpretowane regresje panelu JS oraz Python (bez budowania binariów). Nowy test C++ `RPi_controls_and_background_state` przygotowano, ale **nie uruchomiono**. Nie uruchomiono CMake, kompilatora, CTest, QEMU, Actions ani nowych pomiarów JACK. Nie wydano nowej paczki i nie zainstalowano poprawki u właściciela.

Po zgodzie na kompilację: najpierw nowy host/metadata i zapis stanu; następnie porównanie prawdziwego sygnału RPi/JACK z referencją VST dla tego samego presetu/IR, 44,1/48/96 kHz, obu kanałów, poziomów i różnych buforów; pomiar reakcji CC i liczników xrun/CPU. Dopiero to pozwoli ocenić brzmienie i realną latencję. Attack/Atomic zachowują opóźnienie FFT oryginałów; nie usuwamy go kosztem działającego brzmienia VST.

## Tape Drive / Cloud — robocza zmiana DSP, nadal bez kompilacji

Uwaga właściciela: mechanika TD mało słyszalna, Warm nie daje satysfakcjonującego
wzbogacenia harmonicznych, Cloud brzmi jak standardowy reverb. Wcześniejsze
stwierdzenie o niezmienionym wspólnym DSP dotyczy poprzedniego etapu. Na wyraźne
zlecenie właściciela zmieniono teraz wspólny DSP; przyszły build VST także odziedziczy
te zmiany. Nie zmodyfikowano repozytoriów oryginalnych efektów ani wydanych paczek.

Analiza kodu: Cloud był tą samą ośmioliniową siecią co Plate, z niewielkim
resamplingiem 1.004 i bardzo małym udziałem w feedback. Nie miał osobnego wejściowego
dyfuzora. W TD Warm stosował duże podbicie przed tanh, częściowo znoszone przez
kompresję obwiedni. Mechanika miała niewielką amplitudę, a centrum opóźnienia
przeskakiwało z 0 na 5.8 ms po przekroczeniu progu aktywacji.

Zmiany:
- Warm: osobna asymetryczna miękka charakterystyka z normalizacją wzmocnienia
  małosygnałowego, udziałem czystego sygnału i progresją harmonicznych. Hot zachowuje
  dotychczasowy agresywny tor. Harmoniczne są z definicji nieliniowością; celem jest
  subtelne nasycenie bez ostrego clippingu, nie matematyczny brak zniekształceń.
- Mechanika: zwiększony zakres wow/flutter i losowego dryfu, płynne centrum 0–8 ms,
  zachowane ograniczenia bufora. Jest to dynamiczne opóźnienie efektowe, nie nowa
  deklaracja stałej latencji/PDC. Age i poziom szumów bez zwiększania.
- Cloud: 6 stopni stereofonicznej dyfuzji allpass z ortogonalnymi rotacjami,
  dłuższe opóźnienia sieci, RT60 obliczane z rzeczywistej długości odczytu,
  modulacja skalowana czasowo do sample rate i rozproszone wyjścia stereo.
  Usunięto słaby pitchshift z Cloud; Shimmer nadal używa własnego pitchshift.
  Brak kopiowania kodu Strymon i brak deklaracji emulacji konkretnego produktu.
  Pozostałe tryby reverb nie mają zmienionego toru audio.
- Bufory dyfuzji przydzielane wyłącznie w prepare; reset unieważnia pamięć epoką.
  Brak nowych locków, alokacji i I/O wewnątrz tick.

Weryfikacja wykonana: statyczny przegląd zmian i git diff --check PASS.
Przygotowane, lecz NIE URUCHOMIONE testy C++: pomiar harmonicznych Warm,
bezpieczne poziomy, gęsty ogon i stereo Cloud, reset dyfuzorów dla 44.1/48/96 kHz.
Nadal wymagane po odblokowaniu kompilacji: wykonanie testów, pomiar aliasingu i CPU
na ARM, porównanie głośności/transjentów na gitarze i basie, odsłuch Cloud oraz
płynność automatyzacji i zmiany silników. Poprawka nie jest zainstalowana na Pi.
