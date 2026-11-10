# Pico 2 Pocket Snake

A self-contained C/C++ SDK project for the existing Raspberry Pi Pico 2,
ST7789 SPI TFT, four tactile switches, and passive piezo. Snake is playable on
the 320×240 display, in landscape rotated 180°. The game uses double RGB565
framebuffers, DMA display transfer, the Pico 2's two cores, debounced buttons,
PWM sound, and a persistent high score.

## Build and flash

Set `PICO_SDK_PATH` to your installed `pico-sdk` folder and make sure CMake,
GNU Make, and the ARM GCC toolchain are available. From this `spi_display`
folder, run:

```text
make
```

Flash `build/pico_button_game.uf2` to the Pico 2. CMake also writes
`build/pico_button_game.map` so the actual static RAM/flash footprint can be
reported after building.

## Controls

| Control | Button wiring |
|---|---|
| Up | GP12, physical pin 16 |
| Left | GP13, physical pin 17 |
| Down | GP14, physical pin 19 |
| Right | GP15, physical pin 20 |
| Pause/resume | GP2, physical pin 4 |
| Restart | GP3, physical pin 5 |

There are four physical buttons in the current setup. The pause and restart
Each switch connects its GPIO to common GND when pressed; internal pull-ups
are enabled, so no resistors are needed.

## Existing display and buzzer wiring

| Device signal | Pico 2 GPIO | Physical pin |
|---|---:|---:|
| TFT SCK | GP18 | 24 |
| TFT MOSI / SDI | GP19 | 25 |
| TFT CS | GP17 | 22 |
| TFT DC | GP20 | 26 |
| TFT RESET | GP21 | 27 |
| TFT VCC, LED/BL | 3V3(OUT) | 36 |
| TFT GND | GND | 38 |
| Passive piezo signal/+ | GP11 | 15 |
| Passive piezo other lead | GND | 18 or 38 |

See [docs/schematic.md](docs/schematic.md) for the complete connection diagram
and [docs/measurement_report.md](docs/measurement_report.md) for the report
and measurement worksheet.

## Implementation layout

- `main.c` starts the two-core program and schedules input, game updates, and
  display snapshots.
- `src/game.c` contains Snake movement, food, collision, score, and speed.
- `src/input.c` debounces all six buttons and reads the dedicated pause/restart inputs.
- `src/display_st7789.c` draws into alternating framebuffers and transfers
  each by DMA; drawing overlaps the previous frame's transfer.
- `src/sound.c` makes movement, food, loss, and win tones with PWM.
- `src/score_store.c` stores a checksummed best score in a reserved flash
  sector, writing only after a run ends with a new record.

The game starts on power-up. Use the direction buttons to steer the snake to
the red food; the snake grows and speeds up as the score increases. Hitting a
wall or the snake's body ends the run. High score survives power-off.
