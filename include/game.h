#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "console_config.h"
#include "input.h"

typedef enum {
    GAME_PLAYING,
    GAME_PAUSED,
    GAME_OVER,
    GAME_WON
} GameStatus;

typedef enum {
    DIR_UP,
    DIR_RIGHT,
    DIR_DOWN,
    DIR_LEFT
} Direction;

typedef enum {
    GAME_EVENT_NONE,
    GAME_EVENT_MOVE,
    GAME_EVENT_EAT,
    GAME_EVENT_LOST,
    GAME_EVENT_WON
} GameEvent;

typedef struct {
    uint8_t snake_x[GAME_MAX_SNAKE];
    uint8_t snake_y[GAME_MAX_SNAKE];
    uint16_t length;
    uint8_t food_x;
    uint8_t food_y;
    Direction direction;
    GameStatus status;
    uint32_t score;
    uint32_t best_score;
    uint32_t rng;
    uint32_t last_step_ms;
    bool score_dirty;
} GameState;

void game_init(GameState *game, uint32_t saved_best, uint32_t random_seed);
void game_apply_input(GameState *game, const InputFrame *input);
GameEvent game_update(GameState *game, uint32_t now_ms);

