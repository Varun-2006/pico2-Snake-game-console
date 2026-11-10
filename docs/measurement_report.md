# Pocket Snake project report and measurement worksheet

**Hardware:** Raspberry Pi Pico 2, existing 320×240 ST7789 SPI TFT, four
normally-open tactile switches, passive piezo, USB power. No additional
electronics are required by the software.

**Status:** Source and build configuration are prepared. Physical operation,
FPS, linker memory totals, and USB power have not been measured in this
workspace. Fill the measurement fields after flashing and testing; do not
report the theoretical figures below as measured results.

## Design summary

Snake is played with the four directional buttons. The snake eats red food,
grows, earns 10 points per food, speeds up with score, and loses on wall or
self collision. UP+DOWN held together pauses/resumes; LEFT+RIGHT held together
restarts. The best score is written to a reserved internal-flash sector when
a run ends, with a small record and integrity check.

Core 0 samples/debounces controls and advances the game at a 1 ms loop rate.
It publishes the latest game state to a one-entry queue. Core 1 uses two full
RGB565 framebuffers: while SPI TX DMA sends one buffer, Core 1 draws the next
frame into the other. Old queued states are replaced so input is not blocked
by display refresh. The displayed FPS is measured from completed transfers.

Movement and food/loss/win events use PWM tones on GP11. The design is USB
powered and does not use a battery.

## Memory and throughput estimates

- Two 320×240 RGB565 framebuffers: `2 × 320 × 240 × 2 = 307,200 bytes`
  (300 KiB), about 58% of the Pico 2's 520 KiB SRAM. This is the largest
  static RAM allocation.
- Snake coordinates use up to `2 × 32 × 22 = 1,408 bytes`; game state is
  copied through a one-entry render queue and a core-1 snapshot.
- The exact total SRAM and flash usage should be read from
  `build/pico_button_game.map` or the ELF size report after the SDK build.
- The SPI clock is requested at 75 MHz. The SPI driver returns the actual
  configured baud rate; use that value when calculating the raw full-frame
  transfer ceiling: `actual_SPI_bits_per_second ÷ (153,600 × 8)`.
- The on-screen FPS counter is the project result to record. It includes
  drawing and DMA transfer, so it will be below the raw bus ceiling.

## Measurements to complete on the physical console

| Measurement | Result | Method |
|---|---|---|
| Build / SDK version | Pending | Record the local Pico SDK version and build result |
| Actual SPI clock | Pending | Hold UP+DOWN to pause and read the SPI MHz line |
| Average gameplay FPS | Pending | Read the on-screen FPS after 30 seconds of play |
| Lowest observed FPS | Pending | Observe the counter during rapid movement and score updates |
| ELF flash usage | Pending | `arm-none-eabi-size build/pico_button_game.elf` |
| Static RAM usage | Pending | ELF size or linker map; include framebuffer |
| USB voltage/current | Not measured | Requires an existing inline USB meter; software cannot infer board current |
| Energy for 10 minutes | Pending | `Wh = average_voltage × average_current × 10/60` |
| Button bounce/missed input | Pending | 100 presses per direction and 20 chord presses; record missed/double actions |

The current setup has no USB current sensor and no charging/measurement IC, so
the software cannot produce an honest current or energy reading. Do not fill
these fields with guessed values.

## Baseline and scope limits

- The baseline calls for six physical buttons. This build has the four already
  wired; pause and restart are chord controls, not two independent buttons.
- A handheld enclosure is not present in the available hardware, so the
  breadboard setup is not yet a finished handheld enclosure.
- Sleep-on-close needs a lid/close sensor or a physical close switch. With no
  added hardware, it is not implemented; the pause chord is available instead.
- Multiconsole play needs a second console and a communications link, neither
  of which is in the current setup.
- Sound and DMA are implemented; physical sound quality and high-speed SPI
  signal integrity still need bench verification.

## Demo video checklist

Record the assembled console booting, move the snake in all four directions,
collect food, show the score rising and the FPS overlay, pause/resume with
UP+DOWN, restart with LEFT+RIGHT, then power-cycle and show the saved best
score. Narrate the measured FPS, memory totals, and power readings only after
you have recorded them above.
