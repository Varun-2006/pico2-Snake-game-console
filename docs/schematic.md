# Wiring schematic

This wiring uses only the parts already in the project: Raspberry Pi Pico 2,
ST7789 SPI TFT, four normally-open tactile switches, and one passive piezo.
Display orientation is 320×240 landscape, rotated 180° in software.

## Connection table

| Pico 2 pin | Connect to | Notes |
|---|---|---|
| 15 / GP11 | Piezo + / signal | PWM sound output |
| 16 / GP12 | Up button | Internal pull-up; switch to GND |
| 17 / GP13 | Left button | Internal pull-up; switch to GND |
| 18 / GND | Common ground rail | Shared by buttons, piezo, and TFT |
| 19 / GP14 | Down button | Internal pull-up; switch to GND |
| 20 / GP15 | Right button | Internal pull-up; switch to GND |
| 22 / GP17 | TFT CS | SPI chip select |
| 24 / GP18 | TFT SCK | SPI clock |
| 25 / GP19 | TFT SDI / MOSI | SPI data from Pico to TFT |
| 26 / GP20 | TFT DC | Data/command select |
| 27 / GP21 | TFT RESET | Controller reset |
| 36 / 3V3(OUT) | TFT VCC and LED/BL | Existing display power wiring |
| 38 / GND | TFT GND | Common ground |
| 38 / GND | Piezo − / other lead | Passive piezo return |

## Simplified diagram

```text
Pico 2                                  ST7789 TFT
GP18 (pin 24) ------------------------- SCK
GP19 (pin 25) ------------------------- SDI / MOSI
GP17 (pin 22) ------------------------- CS
GP20 (pin 26) ------------------------- DC
GP21 (pin 27) ------------------------- RESET
3V3  (pin 36) ------------------------- VCC, LED/BL
GND  (pin 38) ------------------------- GND

Pico 2                                  Four momentary buttons
GP12 (pin 16) ----[ UP switch ]----+
GP13 (pin 17) ----[ LEFT switch ]--+
GP14 (pin 19) ----[ DOWN switch ]--+---- GND (pin 18 or 38)
GP15 (pin 20) ----[ RIGHT switch ]-+

Pico 2                                  Passive piezo
GP11 (pin 15) ------------------------- + / signal
GND  (pin 18 or 38) ------------------- − / other lead
```

For each four-leg tactile switch, its two legs on one electrical side are
already joined. Put the GPIO on one switch side and common GND on the other
side; do not use two legs from the same internally connected pair as the two
connections. All switch grounds can share the same ground rail.

## Logical controls with four switches

No extra buttons are present in the current hardware. Holding UP+DOWN for
about half a second toggles pause. Holding LEFT+RIGHT for about half a second
restarts the round. The input module filters contact bounce for 25 ms and
waits briefly before treating a single direction press as movement, allowing
opposite-button chords to be recognized.
