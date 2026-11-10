#include "input.h"
#include "console_config.h"

typedef struct {
    uint pin;
    bool raw_high;
    bool stable_high;
    bool direction_sent;
    absolute_time_t last_change;
    absolute_time_t stable_since;
} DebouncedButton;

typedef struct {
    bool active;
    bool fired;
    absolute_time_t started;
} ChordTimer;

enum { BTN_UP, BTN_LEFT, BTN_DOWN, BTN_RIGHT, BTN_PAUSE, BTN_RESTART, BUTTON_COUNT };
static DebouncedButton buttons[BUTTON_COUNT];

static void init_one_button(DebouncedButton *button, uint pin) {
    button->pin = pin;
    gpio_init(pin);
    gpio_set_dir(pin, GPIO_IN);
    gpio_pull_up(pin);
    button->raw_high = gpio_get(pin);
    button->stable_high = button->raw_high;
    button->direction_sent = false;
    button->last_change = get_absolute_time();
    button->stable_since = button->last_change;
}

static void update_one_button(DebouncedButton *button, absolute_time_t now) {
    bool sample = gpio_get(button->pin);
    if (sample != button->raw_high) {
        button->raw_high = sample;
        button->last_change = now;
    }

    if (button->raw_high != button->stable_high &&
        absolute_time_diff_us(button->last_change, now) >= INPUT_DEBOUNCE_US) {
        button->stable_high = button->raw_high;
        button->stable_since = now;
        button->direction_sent = false;
    }
}

static bool chord_held(DebouncedButton *first, DebouncedButton *second) {
    return !first->stable_high && !second->stable_high;
}

static bool update_chord(ChordTimer *chord, bool held, absolute_time_t now) {
    if (!held) {
        chord->active = false;
        chord->fired = false;
        return false;
    }
    if (!chord->active) {
        chord->active = true;
        chord->fired = false;
        chord->started = now;
    }
    if (!chord->fired &&
        absolute_time_diff_us(chord->started, now) >= INPUT_CHORD_HOLD_US) {
        chord->fired = true;
        return true;
    }
    return false;
}

void input_init(void) {
    init_one_button(&buttons[BTN_UP], BUTTON_PIN_UP);
    init_one_button(&buttons[BTN_LEFT], BUTTON_PIN_LEFT);
    init_one_button(&buttons[BTN_DOWN], BUTTON_PIN_DOWN);
    init_one_button(&buttons[BTN_RIGHT], BUTTON_PIN_RIGHT);
    init_one_button(&buttons[BTN_PAUSE], BUTTON_PIN_PAUSE);
    init_one_button(&buttons[BTN_RESTART], BUTTON_PIN_RESTART);
}

void input_poll(InputFrame *frame) {
    absolute_time_t now = get_absolute_time();
    for (unsigned i = 0; i < BUTTON_COUNT; ++i) {
        update_one_button(&buttons[i], now);
    }

    frame->pause_toggle = !buttons[BTN_PAUSE].stable_high && !buttons[BTN_PAUSE].direction_sent;
    frame->restart = !buttons[BTN_RESTART].stable_high && !buttons[BTN_RESTART].direction_sent;
    if (frame->pause_toggle) buttons[BTN_PAUSE].direction_sent = true;
    if (frame->restart) buttons[BTN_RESTART].direction_sent = true;

    // Delay a direction briefly so an opposite-button chord can be recognized
    // without turning the snake first.
    frame->up = !buttons[BTN_UP].stable_high &&
        !buttons[BTN_UP].direction_sent &&
        absolute_time_diff_us(buttons[BTN_UP].stable_since, now) >= INPUT_CHORD_GRACE_US;
    frame->down = !buttons[BTN_DOWN].stable_high &&
        !buttons[BTN_DOWN].direction_sent &&
        absolute_time_diff_us(buttons[BTN_DOWN].stable_since, now) >= INPUT_CHORD_GRACE_US;
    frame->left = !buttons[BTN_LEFT].stable_high &&
        !buttons[BTN_LEFT].direction_sent &&
        absolute_time_diff_us(buttons[BTN_LEFT].stable_since, now) >= INPUT_CHORD_GRACE_US;
    frame->right = !buttons[BTN_RIGHT].stable_high &&
        !buttons[BTN_RIGHT].direction_sent &&
        absolute_time_diff_us(buttons[BTN_RIGHT].stable_since, now) >= INPUT_CHORD_GRACE_US;

    if (frame->up) buttons[BTN_UP].direction_sent = true;
    if (frame->down) buttons[BTN_DOWN].direction_sent = true;
    if (frame->left) buttons[BTN_LEFT].direction_sent = true;
    if (frame->right) buttons[BTN_RIGHT].direction_sent = true;
}
