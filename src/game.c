#include <string.h>
#include "game.h"

#define START_LENGTH 4u
#define START_INTERVAL_MS 170u
#define MIN_INTERVAL_MS 85u

static uint32_t random_next(GameState *game) {
    // Small deterministic PRNG; the initial seed comes from the Pico timer.
    uint32_t x = game->rng;
    if (x == 0) x = 0xA341316Cu;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    game->rng = x;
    return x;
}

static bool cell_occupied(const GameState *game, uint8_t x, uint8_t y) {
    for (uint16_t i = 0; i < game->length; ++i) {
        if (game->snake_x[i] == x && game->snake_y[i] == y) return true;
    }
    return false;
}

static bool spawn_food(GameState *game) {
    const uint32_t cells = GAME_COLUMNS * GAME_ROWS;
    for (uint32_t attempt = 0; attempt < cells; ++attempt) {
        uint32_t index = random_next(game) % cells;
        uint8_t x = (uint8_t)(index % GAME_COLUMNS);
        uint8_t y = (uint8_t)(index / GAME_COLUMNS);
        if (!cell_occupied(game, x, y)) {
            game->food_x = x;
            game->food_y = y;
            return true;
        }
    }
    game->status = GAME_WON;
    return false;
}

static void start_round(GameState *game) {
    uint32_t best = game->best_score;
    uint32_t rng = game->rng;
    memset(game->snake_x, 0, sizeof game->snake_x);
    memset(game->snake_y, 0, sizeof game->snake_y);
    game->length = START_LENGTH;
    game->snake_x[0] = GAME_COLUMNS / 2;
    game->snake_y[0] = GAME_ROWS / 2;
    for (uint16_t i = 1; i < START_LENGTH; ++i) {
        game->snake_x[i] = (uint8_t)(GAME_COLUMNS / 2 - i);
        game->snake_y[i] = GAME_ROWS / 2;
    }
    game->direction = DIR_RIGHT;
    game->status = GAME_PLAYING;
    game->score = 0;
    game->best_score = best;
    game->rng = rng;
    game->score_dirty = false;
    game->last_step_ms = 0;
    (void)spawn_food(game);
}

void game_init(GameState *game, uint32_t saved_best, uint32_t random_seed) {
    memset(game, 0, sizeof *game);
    game->best_score = saved_best;
    game->rng = random_seed ? random_seed : 0x1F123BB5u;
    start_round(game);
}

static bool is_opposite(Direction a, Direction b) {
    return ((a == DIR_UP && b == DIR_DOWN) ||
            (a == DIR_DOWN && b == DIR_UP) ||
            (a == DIR_LEFT && b == DIR_RIGHT) ||
            (a == DIR_RIGHT && b == DIR_LEFT));
}

void game_apply_input(GameState *game, const InputFrame *input) {
    if (input->restart) {
        start_round(game);
        return;
    }

    if (input->pause_toggle &&
        (game->status == GAME_PLAYING || game->status == GAME_PAUSED)) {
        game->status = game->status == GAME_PLAYING ? GAME_PAUSED : GAME_PLAYING;
        game->last_step_ms = 0;
    }

    if (game->status != GAME_PLAYING) return;

    Direction requested = game->direction;
    bool has_request = false;
    if (input->up) { requested = DIR_UP; has_request = true; }
    if (input->down) { requested = DIR_DOWN; has_request = true; }
    if (input->left) { requested = DIR_LEFT; has_request = true; }
    if (input->right) { requested = DIR_RIGHT; has_request = true; }
    if (has_request && !is_opposite(requested, game->direction)) {
        game->direction = requested;
    }
}

static void end_round(GameState *game, GameStatus status) {
    game->status = status;
}

GameEvent game_update(GameState *game, uint32_t now_ms) {
    if (game->status != GAME_PLAYING) return GAME_EVENT_NONE;

    uint32_t speedup = (game->score / 50u) * 8u;
    uint32_t interval = speedup >= START_INTERVAL_MS - MIN_INTERVAL_MS
                            ? MIN_INTERVAL_MS
                            : START_INTERVAL_MS - speedup;
    if (game->last_step_ms == 0) game->last_step_ms = now_ms;
    if ((uint32_t)(now_ms - game->last_step_ms) < interval) {
        return GAME_EVENT_NONE;
    }
    game->last_step_ms = now_ms;

    int16_t next_x = game->snake_x[0];
    int16_t next_y = game->snake_y[0];
    switch (game->direction) {
        case DIR_UP: --next_y; break;
        case DIR_RIGHT: ++next_x; break;
        case DIR_DOWN: ++next_y; break;
        case DIR_LEFT: --next_x; break;
    }

    if (next_x < 0 || next_x >= (int16_t)GAME_COLUMNS ||
        next_y < 0 || next_y >= (int16_t)GAME_ROWS) {
        end_round(game, GAME_OVER);
        return GAME_EVENT_LOST;
    }

    bool eating = next_x == game->food_x && next_y == game->food_y;
    for (uint16_t i = 0; i < game->length; ++i) {
        // The tail moves away on a normal step, so entering that cell is safe.
        if (!eating && i == game->length - 1) continue;
        if (game->snake_x[i] == (uint8_t)next_x &&
            game->snake_y[i] == (uint8_t)next_y) {
            end_round(game, GAME_OVER);
            return GAME_EVENT_LOST;
        }
    }

    if (eating && game->length == GAME_MAX_SNAKE) {
        end_round(game, GAME_WON);
        return GAME_EVENT_WON;
    }

    uint16_t new_length = game->length + (eating ? 1u : 0u);
    for (uint16_t i = new_length - 1; i > 0; --i) {
        game->snake_x[i] = game->snake_x[i - 1];
        game->snake_y[i] = game->snake_y[i - 1];
    }
    game->length = new_length;
    game->snake_x[0] = (uint8_t)next_x;
    game->snake_y[0] = (uint8_t)next_y;

    if (!eating) return GAME_EVENT_MOVE;

    game->score += 10;
    if (game->score > game->best_score) {
        game->best_score = game->score;
        game->score_dirty = true;
    }
    if (!spawn_food(game)) return GAME_EVENT_WON;
    return GAME_EVENT_EAT;
}
