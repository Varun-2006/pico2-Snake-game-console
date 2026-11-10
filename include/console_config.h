#pragma once

#include <stdint.h>

// Raspberry Pi Pico 2 and the existing ST7789/button/buzzer wiring.
#define TFT_SPI spi0
#define TFT_PIN_SCK 18       // physical pin 24
#define TFT_PIN_MOSI 19      // physical pin 25
#define TFT_PIN_CS 17        // physical pin 22
#define TFT_PIN_DC 20        // physical pin 26
#define TFT_PIN_RESET 21     // physical pin 27

#define BUTTON_PIN_UP 12     // physical pin 16
#define BUTTON_PIN_LEFT 13   // physical pin 17
#define BUTTON_PIN_DOWN 14   // physical pin 19
#define BUTTON_PIN_RIGHT 15  // physical pin 20
#define BUTTON_PIN_PAUSE 2   // physical pin 4
#define BUTTON_PIN_RESTART 3 // physical pin 5
#define BUZZER_PIN 11        // physical pin 15

#define SCREEN_WIDTH 320u
#define SCREEN_HEIGHT 240u
#define FRAMEBUFFER_BYTES (SCREEN_WIDTH * SCREEN_HEIGHT * 2u)
#define DISPLAY_SPI_HZ 75000000u

#define GAME_CELL 10u
#define GAME_HUD_HEIGHT 20u
#define GAME_COLUMNS (SCREEN_WIDTH / GAME_CELL)
#define GAME_ROWS ((SCREEN_HEIGHT - GAME_HUD_HEIGHT) / GAME_CELL)
#define GAME_MAX_SNAKE (GAME_COLUMNS * GAME_ROWS)

#define COLOR_BACKGROUND 0x0841u
#define COLOR_PANEL 0x0000u
#define COLOR_GRID 0x10A2u
#define COLOR_TEXT 0xFFFFu
#define COLOR_ACCENT 0xFFE0u
#define COLOR_SNAKE 0x07E0u
#define COLOR_HEAD 0xBFE0u
#define COLOR_FOOD 0xF800u

#define INPUT_DEBOUNCE_US 25000u
#define INPUT_CHORD_GRACE_US 80000u
#define INPUT_CHORD_HOLD_US 500000u

