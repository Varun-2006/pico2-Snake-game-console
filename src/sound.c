#include "sound.h"
#include "console_config.h"
#include "hardware/clocks.h"
#include "hardware/pwm.h"

static uint buzzer_slice;
static uint16_t current_wrap;
static bool tone_active;
static absolute_time_t tone_end;

static void tone_start(uint frequency_hz, uint duration_ms) {
    uint32_t period = clock_get_hz(clk_sys) / (2u * frequency_hz);
    if (period < 2u) period = 2u;
    if (period > 65536u) period = 65536u;
    current_wrap = (uint16_t)(period - 1u);
    pwm_set_wrap(buzzer_slice, current_wrap);
    pwm_set_gpio_level(BUZZER_PIN, (current_wrap + 1u) / 2u);
    tone_end = make_timeout_time_ms(duration_ms);
    tone_active = true;
}

void sound_init(void) {
    gpio_set_function(BUZZER_PIN, GPIO_FUNC_PWM);
    buzzer_slice = pwm_gpio_to_slice_num(BUZZER_PIN);
    pwm_config config = pwm_get_default_config();
    pwm_config_set_clkdiv(&config, 2.0f);
    current_wrap = 37499; // 2 kHz at the Pico 2 default 150 MHz system clock.
    pwm_config_set_wrap(&config, current_wrap);
    pwm_init(buzzer_slice, &config, true);
    pwm_set_gpio_level(BUZZER_PIN, 0);
}

void sound_play_event(GameEvent event) {
    switch (event) {
        case GAME_EVENT_MOVE:
            tone_start(1600, 24);
            break;
        case GAME_EVENT_EAT:
            tone_start(2600, 90);
            break;
        case GAME_EVENT_LOST:
            tone_start(500, 300);
            break;
        case GAME_EVENT_WON:
            tone_start(3200, 120);
            break;
        case GAME_EVENT_NONE:
        default:
            break;
    }
}

void sound_update(void) {
    if (tone_active && time_reached(tone_end)) {
        pwm_set_gpio_level(BUZZER_PIN, 0);
        tone_active = false;
    }
}
