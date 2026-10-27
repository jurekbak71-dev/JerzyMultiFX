# Podłączenie wyświetlacza ze zdjęć

Zidentyfikowany moduł: QAPASS 1602A, opakowanie BotiLand LCD-03336, 16 pinów, bez adaptera I²C. Oznaczenia VSS/VDD/VO/RS/RW/E/D0–D7/A/K są widoczne na zdjęciu przodu. Numer 16 jest nadrukowany przy K. Pin 1 to VSS, na przeciwnym końcu listwy.

Schemat dla Raspberry Pi ze złączem 40-pinowym. **Wszystkie numery pinów Raspberry Pi poniżej to numery fizyczne, nie numery GPIO.** Połączenia wykonuj przy odłączonym zasilaczu Raspberry Pi. Numer 1 identyfikuj na płytce, nie według obrotu zdjęcia. Nie podawaj 5 V na żaden GPIO.

## Potrzebne części

- Przewody oraz płytka stykowa.
- Jeden układ **74AHCT245 w obudowie DIP-20**. Nie zamieniaj na 74HC245; AHCT przyjmuje sygnały 3,3 V przy zasilaniu 5 V.
- Potencjometr 10 kΩ do ustawiania kontrastu.
- Rezystor 220 Ω do podświetlenia.
- Kondensator ceramiczny 100 nF przy układzie 74AHCT245.

## 1. Zasilanie

Z pinu **2 Raspberry Pi (5 V)** poprowadź przewód do szyny „+” płytki stykowej.
Z pinu **6 Raspberry Pi (masa)** poprowadź przewód do szyny „−”.

| Pin wyświetlacza | Dokąd podłączyć |
|---|---|
| 1 — VSS | Szyna „−” |
| 2 — VDD | Szyna „+” |
| 3 — VO | Środkowa nóżka potencjometru 10 kΩ |
| 4 — RS | Nóżka 18 układu 74AHCT245 |
| 5 — RW | Szyna „−” — koniecznie, wyświetlacz nie może wysyłać sygnałów 5 V do Pi |
| 6 — E | Nóżka 17 układu 74AHCT245 |
| 7, 8, 9, 10 — D0–D3 | Nie podłączaj |
| 11 — D4 | Nóżka 16 układu 74AHCT245 |
| 12 — D5 | Nóżka 15 układu 74AHCT245 |
| 13 — D6 | Nóżka 14 układu 74AHCT245 |
| 14 — D7 | Nóżka 13 układu 74AHCT245 |
| 15 — A | Przez rezystor 220 Ω do szyny „+” |
| 16 — K | Szyna „−” |

Dwie skrajne nóżki potencjometru podłącz do szyn „+” i „−”. Obracanie pokrętła zmienia kontrast. Dodatkowy rezystor podświetlenia pozostaje w schemacie nawet jeśli moduł ma już własny; może zmniejszyć jasność.

## 2. Konwerter 74AHCT245

Patrz na układ od góry, z wycięciem u góry. Lewa strona od góry: nóżki 1–10. Prawa strona od dołu do góry: 11–20. Przed połączeniem porównaj oznaczenie i kartę katalogową zakupionego układu: wymagany DIP-20 74AHCT245.

| Nóżka układu | Połączenie |
|---|---|
| 1 — DIR | Szyna „+” — kierunek z Raspberry Pi do wyświetlacza |
| 20 — VCC | Szyna „+” |
| 10 — GND | Szyna „−” |
| 19 — /OE | Szyna „−” |
| 2 — A1 | Raspberry Pi pin **11**, GPIO17 — RS |
| 3 — A2 | Raspberry Pi pin **12**, GPIO18 — E |
| 4 — A3 | Raspberry Pi pin **13**, GPIO27 — D4 |
| 5 — A4 | Raspberry Pi pin **15**, GPIO22 — D5 |
| 6 — A5 | Raspberry Pi pin **16**, GPIO23 — D6 |
| 7 — A6 | Raspberry Pi pin **18**, GPIO24 — D7 |
| 8 i 9 — A7/A8 | Szyna „−” |
| 11 i 12 — B8/B7 | Nie podłączaj |

Kondensator 100 nF podłącz między nóżki 20 i 10, możliwie blisko układu. Nie ma polaryzacji.

Droga sześciu sygnałów:

```text
Raspberry Pi       74AHCT245                  LCD
pin 11 (GPIO17) → 2 (A1) → 18 (B1) → pin  4 (RS)
pin 12 (GPIO18) → 3 (A2) → 17 (B2) → pin  6 (E)
pin 13 (GPIO27) → 4 (A3) → 16 (B3) → pin 11 (D4)
pin 15 (GPIO22) → 5 (A4) → 15 (B4) → pin 12 (D5)
pin 16 (GPIO23) → 6 (A5) → 14 (B5) → pin 13 (D6)
pin 18 (GPIO24) → 7 (A6) → 13 (B6) → pin 14 (D7)
```

## 3. Sprawdzenie przed włączeniem

- Wszystkie masy mają połączenie: Pi, LCD, konwerter i potencjometr.
- LCD RW jest na stałe połączone z masą.
- Żadne 5 V ani wyjście B konwertera nie jest podłączone do GPIO Pi.
- Numery pinów odczytano z właściwej strony obu elementów.
- Nie użyto GPIO zajętych przez inną nakładkę lub istniejące okablowanie.

Po włączeniu podświetlenie może świecić, ale napisy pojawią się dopiero po uruchomieniu sterownika. Przy regulacji kontrastu mogą być widoczne prostokąty. Obecny instalator audio nie uruchamia LCD automatycznie.

## Informacje dla konfiguracji programu

Kolejność BCM w obecnym sterowniku: `[17, 18, 27, 22, 23, 24]` = RS/E/D4/D5/D6/D7. Wybór `/dev/gpiochip*` wymaga sprawdzenia na konkretnym Raspberry Pi; nie ustawiamy go na podstawie fotografii samego LCD.

To schemat przygotowany na podstawie widocznych oznaczeń i jednokierunkowego bufora AHCT. Nie jest raportem z testu elektrycznego ani testu fizycznego modułu. Zdjęcia nie identyfikują kontrolera pod zalewą ani jego progów wejściowych, dlatego nie zalecamy bezpośredniego sterowania 3,3 V przy zasilaniu LCD 5 V. Materiały referencyjne nie są dołączane do publicznych paczek.
