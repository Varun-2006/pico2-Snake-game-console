#pragma once

#include <stdbool.h>
#include "pico/stdlib.h"

typedef struct {
    bool up;
    bool down;
    bool left;
    bool right;
    bool pause_toggle;
    bool restart;
} InputFrame;

void input_init(void);
void input_poll(InputFrame *frame);

