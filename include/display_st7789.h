#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "game.h"

void display_init(void);
uint32_t display_actual_spi_hz(void);
uint8_t *display_framebuffer(uint8_t index);
void display_render(uint8_t *target, const GameState *game, uint32_t fps,
                    bool main_loop_alive, uint8_t main_loop_stage);
void display_start_dma(const uint8_t *framebuffer);
bool display_wait_dma(void);

