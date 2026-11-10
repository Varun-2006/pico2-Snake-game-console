#include <stddef.h>
#include "console_config.h"
#include "display_st7789.h"
#include "hardware/dma.h"
#include "hardware/spi.h"

#define DISPLAY_SPI spi0

// RGB565 pixels are stored in wire byte order (high byte, then low byte).
// Two buffers let Core 1 draw one frame while DMA sends the other.
static uint8_t framebuffers[2][FRAMEBUFFER_BYTES] __attribute__((aligned(4)));
static uint8_t *draw_buffer;
static int dma_channel;
static uint32_t actual_spi_hz;

static const uint8_t snake_head_sprite[8] = {
    0x3C, 0x7E, 0xFF, 0xFF, 0xFF, 0xFF, 0x7E, 0x3C
};
static const uint8_t snake_body_sprite[8] = {
    0x7E, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x7E
};
static const uint8_t food_sprite[8] = {
    0x18, 0x3C, 0x7E, 0x7E, 0x7E, 0x7E, 0x3C, 0x18
};

static const uint8_t digits_3x5[10][5] = {
    {7, 5, 5, 5, 7}, {2, 6, 2, 2, 7}, {7, 1, 7, 4, 7},
    {7, 1, 7, 1, 7}, {5, 5, 7, 1, 1}, {7, 4, 7, 1, 7},
    {7, 4, 7, 5, 7}, {7, 1, 2, 2, 2}, {7, 5, 7, 5, 7},
    {7, 5, 7, 1, 7}
};

static const uint8_t letters_3x5[26][5] = {
    {2, 5, 7, 5, 5}, {6, 5, 6, 5, 6}, {3, 4, 4, 4, 3},
    {6, 5, 5, 5, 6}, {7, 4, 6, 4, 7}, {7, 4, 6, 4, 4},
    {3, 4, 5, 5, 3}, {5, 5, 7, 5, 5}, {7, 2, 2, 2, 7},
    {1, 1, 1, 5, 2}, {5, 5, 6, 5, 5}, {4, 4, 4, 4, 7},
    {5, 7, 7, 5, 5}, {5, 7, 7, 7, 5}, {7, 5, 5, 5, 7},
    {6, 5, 6, 4, 4}, {7, 5, 5, 7, 1}, {6, 5, 6, 5, 5},
    {3, 4, 2, 1, 6}, {7, 2, 2, 2, 2}, {5, 5, 5, 5, 7},
    {5, 5, 5, 5, 2}, {5, 5, 7, 7, 5}, {5, 5, 2, 5, 5},
    {5, 5, 2, 2, 2}, {7, 1, 2, 4, 7}
};

static void command_data(uint8_t command, const uint8_t *data, size_t size) {
    gpio_put(TFT_PIN_CS, 0);
    gpio_put(TFT_PIN_DC, 0);
    spi_write_blocking(DISPLAY_SPI, &command, 1);
    if (size != 0) {
        gpio_put(TFT_PIN_DC, 1);
        spi_write_blocking(DISPLAY_SPI, data, size);
    }
    gpio_put(TFT_PIN_CS, 1);
}

static void command(uint8_t value) {
    command_data(value, NULL, 0);
}

static void set_window(uint16_t x, uint16_t y, uint16_t w, uint16_t h) {
    uint16_t x1 = x + w - 1;
    uint16_t y1 = y + h - 1;
    const uint8_t columns[] = {
        (uint8_t)(x >> 8), (uint8_t)x, (uint8_t)(x1 >> 8), (uint8_t)x1
    };
    const uint8_t rows[] = {
        (uint8_t)(y >> 8), (uint8_t)y, (uint8_t)(y1 >> 8), (uint8_t)y1
    };
    command_data(0x2A, columns, sizeof columns); // CASET
    command_data(0x2B, rows, sizeof rows);       // RASET
    command(0x2C);                               // RAMWR
}

static void put_pixel(uint16_t x, uint16_t y, uint16_t color) {
    if (x >= SCREEN_WIDTH || y >= SCREEN_HEIGHT) return;
    size_t offset = ((size_t)y * SCREEN_WIDTH + x) * 2u;
    draw_buffer[offset] = (uint8_t)(color >> 8);
    draw_buffer[offset + 1] = (uint8_t)color;
}

static void fill_rect(uint16_t x, uint16_t y, uint16_t w, uint16_t h,
                      uint16_t color) {
    if (x >= SCREEN_WIDTH || y >= SCREEN_HEIGHT || w == 0 || h == 0) return;
    if ((uint32_t)x + w > SCREEN_WIDTH) w = SCREEN_WIDTH - x;
    if ((uint32_t)y + h > SCREEN_HEIGHT) h = SCREEN_HEIGHT - y;
    for (uint16_t row = y; row < y + h; ++row) {
        size_t offset = ((size_t)row * SCREEN_WIDTH + x) * 2u;
        for (uint16_t col = 0; col < w; ++col) {
            draw_buffer[offset++] = (uint8_t)(color >> 8);
            draw_buffer[offset++] = (uint8_t)color;
        }
    }
}

static const uint8_t *glyph_for(char ch) {
    static const uint8_t colon[5] = {0, 2, 0, 2, 0};
    static const uint8_t dash[5] = {0, 0, 7, 0, 0};
    static const uint8_t plus[5] = {0, 2, 7, 2, 0};
    static const uint8_t blank[5] = {0, 0, 0, 0, 0};
    if (ch >= '0' && ch <= '9') return digits_3x5[ch - '0'];
    if (ch >= 'A' && ch <= 'Z') return letters_3x5[ch - 'A'];
    if (ch == ':') return colon;
    if (ch == '-') return dash;
    if (ch == '+') return plus;
    return blank;
}

static void draw_text(uint16_t x, uint16_t y, const char *text,
                      uint16_t color, uint8_t scale) {
    while (*text != '\0') {
        const uint8_t *glyph = glyph_for(*text++);
        for (uint8_t row = 0; row < 5; ++row) {
            for (uint8_t col = 0; col < 3; ++col) {
                if (glyph[row] & (1u << (2u - col))) {
                    fill_rect(x + col * scale, y + row * scale,
                              scale, scale, color);
                }
            }
        }
        x += 4u * scale;
    }
}

static void draw_number(uint16_t x, uint16_t y, uint32_t value,
                        uint16_t color, uint8_t scale) {
    char reversed[10];
    uint8_t count = 0;
    do {
        reversed[count++] = (char)('0' + value % 10u);
        value /= 10u;
    } while (value != 0 && count < sizeof reversed);
    while (count != 0) {
        char digit[2] = {reversed[--count], '\0'};
        draw_text(x, y, digit, color, scale);
        x += 4u * scale;
    }
}

static void draw_cell(uint8_t grid_x, uint8_t grid_y, uint16_t color,
                      uint8_t inset) {
    uint16_t x = grid_x * GAME_CELL + inset;
    uint16_t y = GAME_HUD_HEIGHT + grid_y * GAME_CELL + inset;
    uint16_t size = GAME_CELL - inset * 2u;
    fill_rect(x, y, size, size, color);
}

static void draw_sprite8(uint16_t x, uint16_t y, const uint8_t rows[8],
                         uint16_t color) {
    for (uint16_t row = 0; row < 8; ++row) {
        for (uint16_t col = 0; col < 8; ++col) {
            if (rows[row] & (uint8_t)(0x80u >> col)) {
                put_pixel(x + col, y + row, color);
            }
        }
    }
}

void display_init(void) {
    actual_spi_hz = spi_init(DISPLAY_SPI, DISPLAY_SPI_HZ);
    spi_set_format(DISPLAY_SPI, 8, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);
    gpio_set_function(TFT_PIN_SCK, GPIO_FUNC_SPI);
    gpio_set_function(TFT_PIN_MOSI, GPIO_FUNC_SPI);

    gpio_init(TFT_PIN_CS);
    gpio_set_dir(TFT_PIN_CS, GPIO_OUT);
    gpio_put(TFT_PIN_CS, 1);
    gpio_init(TFT_PIN_DC);
    gpio_set_dir(TFT_PIN_DC, GPIO_OUT);
    gpio_put(TFT_PIN_DC, 1);
    gpio_init(TFT_PIN_RESET);
    gpio_set_dir(TFT_PIN_RESET, GPIO_OUT);
    gpio_put(TFT_PIN_RESET, 1);

    gpio_put(TFT_PIN_RESET, 0);
    sleep_ms(20);
    gpio_put(TFT_PIN_RESET, 1);
    sleep_ms(150);
    command(0x01); // SWRESET
    sleep_ms(150);
    command(0x11); // SLPOUT
    sleep_ms(120);
    const uint8_t rgb565[] = {0x55};
    command_data(0x3A, rgb565, sizeof rgb565); // COLMOD
    const uint8_t landscape_reversed[] = {0x60};
    command_data(0x36, landscape_reversed, sizeof landscape_reversed); // MADCTL: opposite landscape orientation
    command(0x20); // INVOFF, matching the working pink test configuration
    command(0x13); // NORON
    sleep_ms(10);
    command(0x29); // DISPON
    sleep_ms(100);

    dma_channel = dma_claim_unused_channel(true);
}

uint32_t display_actual_spi_hz(void) {
    return actual_spi_hz;
}

uint8_t *display_framebuffer(uint8_t index) {
    return framebuffers[index & 1u];
}

void display_render(uint8_t *target, const GameState *game, uint32_t fps,
                    bool main_loop_alive, uint8_t main_loop_stage) {
    draw_buffer = target;
    fill_rect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, COLOR_BACKGROUND);
    fill_rect(0, 0, SCREEN_WIDTH, GAME_HUD_HEIGHT, COLOR_PANEL);
    draw_text(4, 5, "SCORE", COLOR_TEXT, 2);
    draw_number(48, 5, game->score, COLOR_TEXT, 2);
    draw_text(112, 5, "BEST", COLOR_ACCENT, 2);
    draw_number(148, 5, game->best_score, COLOR_ACCENT, 2);
    draw_text(238, 5, "FPS", COLOR_TEXT, 2);
    draw_number(270, 5, fps, COLOR_TEXT, 2);
    fill_rect(306, 6, 8, 8, main_loop_alive ? COLOR_SNAKE : COLOR_FOOD);
    if (!main_loop_alive) {
        fill_rect(118, 82, 84, 52, COLOR_PANEL);
        draw_text(134, 88, "HANG", COLOR_FOOD, 2);
        draw_text(126, 112, "STAGE", COLOR_TEXT, 1);
        draw_number(174, 112, main_loop_stage, COLOR_ACCENT, 1);
    }

    // Fine grid lines make movement and collision cells easy to see.
    for (uint16_t x = 0; x < SCREEN_WIDTH; x += GAME_CELL) {
        for (uint16_t y = GAME_HUD_HEIGHT; y < SCREEN_HEIGHT; ++y) {
            put_pixel(x, y, COLOR_GRID);
        }
    }
    for (uint16_t y = GAME_HUD_HEIGHT; y < SCREEN_HEIGHT; y += GAME_CELL) {
        for (uint16_t x = 0; x < SCREEN_WIDTH; ++x) {
            put_pixel(x, y, COLOR_GRID);
        }
    }

    // A clear closed border marks the lethal edge of the playfield.
    const uint16_t border = COLOR_ACCENT;
    fill_rect(0, GAME_HUD_HEIGHT, SCREEN_WIDTH, 2, border);
    fill_rect(0, SCREEN_HEIGHT - 2, SCREEN_WIDTH, 2, border);
    fill_rect(0, GAME_HUD_HEIGHT, 2, SCREEN_HEIGHT - GAME_HUD_HEIGHT, border);
    fill_rect(SCREEN_WIDTH - 2, GAME_HUD_HEIGHT, 2,
              SCREEN_HEIGHT - GAME_HUD_HEIGHT, border);

    draw_sprite8(game->food_x * GAME_CELL + 1,
                 GAME_HUD_HEIGHT + game->food_y * GAME_CELL + 1,
                 food_sprite, COLOR_FOOD);
    put_pixel(game->food_x * GAME_CELL + 4,
              GAME_HUD_HEIGHT + game->food_y * GAME_CELL + 1, COLOR_SNAKE);
    put_pixel(game->food_x * GAME_CELL + 5,
              GAME_HUD_HEIGHT + game->food_y * GAME_CELL + 2, COLOR_SNAKE);
    for (uint16_t i = game->length; i > 1; --i) {
        draw_sprite8(game->snake_x[i - 1] * GAME_CELL + 1,
                     GAME_HUD_HEIGHT + game->snake_y[i - 1] * GAME_CELL + 1,
                     snake_body_sprite, COLOR_SNAKE);
        put_pixel(game->snake_x[i - 1] * GAME_CELL + 4,
                  GAME_HUD_HEIGHT + game->snake_y[i - 1] * GAME_CELL + 4,
                  COLOR_HEAD);
    }
    if (game->length != 0) {
        draw_sprite8(game->snake_x[0] * GAME_CELL + 1,
                     GAME_HUD_HEIGHT + game->snake_y[0] * GAME_CELL + 1,
                     snake_head_sprite, COLOR_HEAD);
        uint16_t hx = game->snake_x[0] * GAME_CELL;
        uint16_t hy = GAME_HUD_HEIGHT + game->snake_y[0] * GAME_CELL;
        if (game->direction == DIR_UP || game->direction == DIR_DOWN) {
            fill_rect(hx + 3, hy + 3, 2, 2, COLOR_PANEL);
            fill_rect(hx + 6, hy + 3, 2, 2, COLOR_PANEL);
        } else {
            fill_rect(hx + 3, hy + 3, 2, 2, COLOR_PANEL);
            fill_rect(hx + 3, hy + 6, 2, 2, COLOR_PANEL);
        }
    }

    if (game->status == GAME_PAUSED) {
        fill_rect(70, 88, 180, 66, COLOR_PANEL);
        draw_text(102, 96, "PAUSED", COLOR_ACCENT, 3);
        draw_text(80, 116, "PRESS PAUSE", COLOR_TEXT, 1);
        draw_text(132, 136, "SPI", COLOR_TEXT, 1);
        draw_number(148, 136, display_actual_spi_hz() / 1000000u,
                    COLOR_ACCENT, 1);
        draw_text(160, 136, "MHZ", COLOR_TEXT, 1);
    } else if (game->status == GAME_OVER) {
        fill_rect(24, 92, 272, 60, COLOR_PANEL);
        draw_text(98, 100, "GAME OVER", COLOR_TEXT, 3);
        draw_text(100, 126, "PRESS RESTART", COLOR_ACCENT, 2);
    } else if (game->status == GAME_WON) {
        fill_rect(72, 100, 176, 42, COLOR_PANEL);
        draw_text(104, 115, "YOU WIN", COLOR_ACCENT, 3);
        draw_text(100, 140, "PRESS RESTART", COLOR_TEXT, 2);
    }
}

void display_start_dma(const uint8_t *framebuffer) {
    set_window(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
    gpio_put(TFT_PIN_CS, 0);
    gpio_put(TFT_PIN_DC, 1);

    dma_channel_config config = dma_channel_get_default_config(dma_channel);
    channel_config_set_transfer_data_size(&config, DMA_SIZE_8);
    channel_config_set_read_increment(&config, true);
    channel_config_set_write_increment(&config, false);
    channel_config_set_dreq(&config, spi_get_dreq(DISPLAY_SPI, true));
    dma_channel_configure(dma_channel, &config,
                          &spi_get_hw(DISPLAY_SPI)->dr,
                          framebuffer, FRAMEBUFFER_BYTES, true);
}

bool display_wait_dma(void) {
    absolute_time_t transfer_deadline = make_timeout_time_ms(100);
    while (dma_channel_is_busy(dma_channel) &&
           !time_reached(transfer_deadline)) {
        tight_loop_contents();
    }
    if (dma_channel_is_busy(dma_channel)) {
        dma_channel_abort(dma_channel);
        gpio_put(TFT_PIN_CS, 1);
        return false;
    }
    absolute_time_t spi_deadline = make_timeout_time_ms(10);
    while (spi_is_busy(DISPLAY_SPI) && !time_reached(spi_deadline)) {
        tight_loop_contents();
    }
    bool transfer_ok = !spi_is_busy(DISPLAY_SPI);
    gpio_put(TFT_PIN_CS, 1);
    return transfer_ok;
}
