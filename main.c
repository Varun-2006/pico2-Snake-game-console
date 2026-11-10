#include <stdbool.h>
#include <stdint.h>
#include "display_st7789.h"
#include "game.h"
#include "input.h"
#include "score_store.h"
#include "sound.h"
#include "pico/flash.h"
#include "pico/multicore.h"
#include "pico/sem.h"
#include "pico/stdlib.h"
#include "pico/util/queue.h"

#define RENDER_PERIOD_MS 33u

static GameState game;
static GameState discarded_snapshot;
static queue_t render_queue;
static semaphore_t renderer_ready;
static volatile uint32_t main_loop_heartbeat;
static volatile uint8_t main_loop_stage;

// Core 1 owns both framebuffers and display SPI/DMA. Core 0 can continue
// sampling buttons and advancing the game while a frame is being transferred.
static void display_worker(void) {
    flash_safe_execute_core_init();
    sem_release(&renderer_ready);

    GameState snapshot;
    uint32_t fps = 0;
    uint32_t frames = 0;
    uint32_t window_start = time_us_32();
    uint32_t seen_main_heartbeat = main_loop_heartbeat;
    uint32_t last_main_progress = window_start;
    bool have_snapshot = false;
    while (!have_snapshot) {
        if (queue_try_remove(&render_queue, &snapshot)) {
            have_snapshot = true;
        } else {
            tight_loop_contents();
        }
    }

    uint32_t now = time_us_32();
    uint32_t heartbeat = main_loop_heartbeat;
    if (heartbeat != seen_main_heartbeat) {
        seen_main_heartbeat = heartbeat;
        last_main_progress = now;
    }
    bool main_loop_alive = (uint32_t)(now - last_main_progress) < 500000u;
    uint8_t drawing_index = 0;
    display_render(display_framebuffer(drawing_index), &snapshot, fps,
                   main_loop_alive, main_loop_stage);
    display_start_dma(display_framebuffer(drawing_index));

    while (true) {
        // If no update is queued, snapshot retains the most recent state.
        (void)queue_try_remove(&render_queue, &snapshot);

        uint8_t next_index = drawing_index ^ 1u;
        now = time_us_32();
        heartbeat = main_loop_heartbeat;
        if (heartbeat != seen_main_heartbeat) {
            seen_main_heartbeat = heartbeat;
            last_main_progress = now;
        }
        main_loop_alive = (uint32_t)(now - last_main_progress) < 500000u;

        // DMA owns the current buffer while this renders into the other one.
        display_render(display_framebuffer(next_index), &snapshot, fps,
                       main_loop_alive, main_loop_stage);
        if (display_wait_dma()) {
            ++frames;
        }

        now = time_us_32();
        uint32_t elapsed = now - window_start;
        if (elapsed >= 1000000u) {
            fps = (uint32_t)(((uint64_t)frames * 1000000u) / elapsed);
            frames = 0;
            window_start = now;
        }

        display_start_dma(display_framebuffer(next_index));
        drawing_index = next_index;
    }
}

static void publish_latest_state(void) {
    if (!queue_try_add(&render_queue, &game)) {
        // Keep the freshest game state instead of building display latency.
        (void)queue_try_remove(&render_queue, &discarded_snapshot);
        (void)queue_try_add(&render_queue, &game);
    }
}

int main(void) {
    display_init();
    sound_init();
    input_init();

    game_init(&game, score_store_load(), time_us_32() ^ 0x92A76B31u);
    queue_init(&render_queue, sizeof(GameState), 1);
    sem_init(&renderer_ready, 0, 1);
    multicore_launch_core1(display_worker);
    sem_acquire_blocking(&renderer_ready);

    uint32_t last_render_publish = 0;
    bool score_save_attempted = false;
    publish_latest_state();

    while (true) {
        ++main_loop_heartbeat;
        main_loop_stage = 1;
        uint32_t now_ms = time_us_32() / 1000u;
        InputFrame input;
        input_poll(&input);
        main_loop_stage = 2;
        if (input.restart && game.score_dirty) {
            main_loop_stage = 3;
            (void)score_store_save(game.best_score);
            game.score_dirty = false;
        }
        main_loop_stage = 4;
        game_apply_input(&game, &input);

        GameEvent event = game_update(&game, now_ms);
        main_loop_stage = 5;
        sound_play_event(event);
        sound_update();

        main_loop_stage = 6;
        if ((game.status == GAME_OVER || game.status == GAME_WON) &&
            game.score_dirty && !score_save_attempted) {
            (void)score_store_save(game.best_score);
            game.score_dirty = false;
            score_save_attempted = true;
        }
        if (input.restart) score_save_attempted = false;

        main_loop_stage = 7;
        uint32_t now_us = time_us_32();
        if ((uint32_t)(now_us - last_render_publish) >=
            RENDER_PERIOD_MS * 1000u) {
            publish_latest_state();
            last_render_publish = now_us;
        }

        main_loop_stage = 8;
        tight_loop_contents();
    }
}
